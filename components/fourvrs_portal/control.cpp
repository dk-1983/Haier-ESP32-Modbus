#include "fourvrs_portal.h"
#include <cstdlib>
namespace esphome { namespace fourvrs_portal {
static bool allowed(const String &value, std::initializer_list<const char *> values) {
  for (auto v : values) if (value == v) return true;
  return false;
}
bool Portal::command_matches_() const {
  if(modbus_pending_) return modbus_matches_();
  if (desired_quiet_>=0 && (!raw_control_valid_ || raw_control_.quiet_mode!=desired_quiet_)) return false;
  if (desired_display_>=0 && (!raw_control_valid_ || raw_control_.display_status!=desired_display_)) return false;
  if (desired_vertical_>=0 && (!raw_control_valid_ || raw_control_.vertical_swing_mode!=desired_vertical_)) return false;
  if (desired_horizontal_>=0 && (!raw_control_valid_ || raw_control_.horizontal_swing_mode!=desired_horizontal_)) return false;
  if (desired_target_set_ && climate_->target_temperature != desired_target_) return false;
  if (desired_mode_.length() && desired_mode_ != log_string_(climate::climate_mode_to_string(climate_->mode))) return false;
  if (desired_fan_.length() && (!climate_->fan_mode.has_value() || desired_fan_ != log_string_(climate::climate_fan_mode_to_string(*climate_->fan_mode)))) return false;
  if (desired_swing_.length() && desired_swing_ != swing_()) return false;
  if (desired_preset_.length() && desired_preset_ != preset_()) return false;
  return true;
}
void Portal::test_command_() {
  if (!test_auth_()) return;
  if (web_.arg("token") != token_) { web_.send(403,"text/plain","Invalid token"); return; }
  String rid=web_.arg("request_id");
  if (rid.length()==0 || rid.length()>40) { web_.send(400,"text/plain","request_id required, max40"); return; }
  if (rid == request_id_) { web_.send(200,"application/json",climate_status_()); return; }
  if (test_pending_) { web_.send(409,"text/plain","Command pending; read status"); return; }
  for (unsigned i=0;i<web_.args();++i)
    if (!allowed(web_.argName(i),{"token","request_id","target","mode","fan","swing","preset"})) {
      web_.send(400,"text/plain","Unknown field"); return;
    }
  String mode=web_.arg("mode"),fan=web_.arg("fan"),swing=web_.arg("swing"),preset=web_.arg("preset");
  bool has_target=web_.hasArg("target"); float target=NAN;
  if (has_target) {
    String raw=web_.arg("target");char *end=nullptr; target=strtof(raw.c_str(),&end);
    if (!raw.length() || end != raw.c_str()+raw.length() || !std::isfinite(target) || target<16 || target>30 || floorf(target)!=target) {
      web_.send(400,"text/plain","Target must be integer16..30 C"); return;
    }
  }
  if ((web_.hasArg("mode") && !allowed(mode,{"OFF","COOL","HEAT","DRY","FAN_ONLY","HEAT_COOL"})) ||
      (web_.hasArg("fan") && !allowed(fan,{"AUTO","LOW","MEDIUM","HIGH"})) ||
      (web_.hasArg("swing") && !allowed(swing,{"OFF","VERTICAL","HORIZONTAL","BOTH"})) ||
      (web_.hasArg("preset") && !allowed(preset,{"NONE","BOOST","SLEEP"})) ||
      (!has_target && !mode.length() && !fan.length() && !swing.length() && !preset.length())) {
    web_.send(400,"text/plain","Unsupported value or empty command"); return;
  }
  if ((mode=="FAN_ONLY" || (!mode.length() && climate_->mode==climate::CLIMATE_MODE_FAN_ONLY)) && fan=="AUTO") {
    web_.send(400,"text/plain","AUTO fan is not supported in FAN_ONLY"); return;
  }
  if (!seen_status_ || !climate_->valid_connection() || uint32_t(millis()-last_status_)>10000) {
    web_.send(409,"text/plain","No fresh Haier status"); return;
  }
  modbus_pending_=false;
  desired_quiet_=desired_display_=desired_vertical_=desired_horizontal_=-1;
  desired_mode_=mode;desired_fan_=fan;desired_swing_=swing;desired_preset_=preset;
  desired_target_set_=has_target;desired_target_=target;request_id_=rid;
  test_pending_=true;control_window_=true;test_started_=millis();test_frame_=status_count_;
  test_state_="pending";test_matches_=0;
  hon_()->set_control_method(haier::HonControlMethod::SET_GROUP_PARAMETERS);
  auto call=climate_->make_call();
  if(has_target)call.set_target_temperature(target);
  if(mode.length())call.set_mode(std::string(mode.c_str()));
  if(fan.length())call.set_fan_mode(std::string(fan.c_str()));
  if(swing.length())call.set_swing_mode(std::string(swing.c_str()));
  if(preset.length())call.set_preset(std::string(preset.c_str()));
  call.perform();
  web_.send(202,"application/json","{\"accepted\":true,\"confirmed\":false}");
}
}}
