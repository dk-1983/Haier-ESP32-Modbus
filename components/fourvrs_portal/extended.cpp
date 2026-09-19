#include "fourvrs_portal.h"
namespace esphome { namespace fourvrs_portal {
int Portal::position_value_(const String &name, bool vertical) {
  using namespace haier::hon_protocol;
  if (vertical) {
    if(name=="HEALTH_UP")return int(VerticalSwingMode::HEALTH_UP);
    if(name=="MAX_UP")return int(VerticalSwingMode::MAX_UP);
    if(name=="UP")return int(VerticalSwingMode::UP);
    if(name=="CENTER")return int(VerticalSwingMode::CENTER);
    if(name=="DOWN")return int(VerticalSwingMode::DOWN);
    if(name=="HEALTH_DOWN")return int(VerticalSwingMode::HEALTH_DOWN);
  } else {
    if(name=="MAX_LEFT")return int(HorizontalSwingMode::MAX_LEFT);
    if(name=="LEFT")return int(HorizontalSwingMode::LEFT);
    if(name=="CENTER")return int(HorizontalSwingMode::CENTER);
    if(name=="RIGHT")return int(HorizontalSwingMode::RIGHT);
    if(name=="MAX_RIGHT")return int(HorizontalSwingMode::MAX_RIGHT);
  }
  return -1;
}
String Portal::position_name_(int value, bool vertical) {
  for (const char *name : {"HEALTH_UP","MAX_UP","UP","CENTER","DOWN","HEALTH_DOWN","MAX_LEFT","LEFT","RIGHT","MAX_RIGHT"})
    if(position_value_(name,vertical)==value)return name;
  if(value==(vertical ? 12 : 7))return "AUTO";
  if(vertical && value==14)return "AUTO_SPECIAL";
  if(vertical && value==10)return "MAX_DOWN";
  return String("UNKNOWN_")+String(value);
}
void Portal::extended_command_() {
  if(!test_auth_())return;
  if(web_.arg("token")!=token_){web_.send(403,"text/plain","Invalid token");return;}
  String rid=web_.arg("request_id");
  if(rid.length()==0 || rid.length()>40){web_.send(400,"text/plain","Invalid request_id");return;}
  if(rid==request_id_){web_.send(200,"application/json",climate_status_());return;}
  if(test_pending_){web_.send(409,"text/plain","Command pending");return;}
  String field,value;unsigned count=0;
  for(unsigned i=0;i<web_.args();++i){
    String name=web_.argName(i);
    if(name=="token" || name=="request_id")continue;
    if(name!="quiet" && name!="display" && name!="vertical_position" && name!="horizontal_position"){
      web_.send(400,"text/plain","Unknown field");return;
    }
    field=name;value=web_.arg(i);++count;
  }
  if(count!=1){web_.send(400,"text/plain","Send exactly one setting");return;}
  bool boolean=field=="quiet" || field=="display";
  int selected=boolean ? (value=="ON" ? 1 : value=="OFF" ? 0 : -1) : position_value_(value,field=="vertical_position");
  if(selected<0){web_.send(400,"text/plain","Unsupported value");return;}
  if(!seen_status_ || !raw_control_valid_ || !climate_->valid_connection() || uint32_t(millis()-last_status_)>10000){
    web_.send(409,"text/plain","No fresh Haier status");return;
  }
  if(field=="quiet" && (climate_->mode==climate::CLIMATE_MODE_OFF || climate_->mode==climate::CLIMATE_MODE_FAN_ONLY || (selected && preset_()=="BOOST"))){
    web_.send(409,"text/plain","Quiet requires active non-fan mode without BOOST");return;
  }
  modbus_pending_=false;
  desired_mode_=desired_fan_=desired_swing_=desired_preset_="";desired_target_set_=false;
  desired_quiet_=desired_display_=desired_vertical_=desired_horizontal_=-1;
  request_id_=rid;test_pending_=true;control_window_=true;test_started_=millis();test_frame_=status_count_;test_matches_=0;test_state_="pending";
  hon_()->set_control_method(haier::HonControlMethod::SET_GROUP_PARAMETERS);
  if(field=="quiet"){desired_quiet_=selected;hon_()->set_quiet_mode_state(selected);}
  if(field=="display"){desired_display_=selected;climate_->set_display_state(selected);}
  if(field=="vertical_position"){desired_vertical_=selected;hon_()->set_vertical_airflow(static_cast<haier::hon_protocol::VerticalSwingMode>(selected));}
  if(field=="horizontal_position"){desired_horizontal_=selected;hon_()->set_horizontal_airflow(static_cast<haier::hon_protocol::HorizontalSwingMode>(selected));}
  web_.send(202,"application/json","{\"accepted\":true,\"confirmed\":false}");
}
}}
