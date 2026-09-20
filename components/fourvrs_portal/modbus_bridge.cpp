// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
#include "register_model.h"
#include <algorithm>
#include <cmath>
namespace esphome { namespace fourvrs_portal {
using haier_bridge::Table;
using haier_bridge::Change;
using haier_bridge::mode_ycj;
using haier_bridge::fan_ycj;
uint8_t Portal::read(Table table,uint16_t address,uint16_t &value) {
  if((table==Table::COIL && address>2)||(table==Table::HOLDING && address>7)||(table==Table::INPUT_REGISTER && address>8))return 2;
  bool fresh=seen_status_ && raw_control_valid_ && climate_->valid_connection() && uint32_t(millis()-last_status_)<30000;
  if(table==Table::INPUT_REGISTER && address>=4){
    switch(address){
      case 4:value=seen_status_?std::min<uint32_t>((millis()-last_status_)/1000,65535):65535;break;
      case 5:value=status_count_&0xffff;break;
      case 6:value=status_count_>>16;break;
      case 7:value=test_pending_?1:test_state_=="confirmed"?2:test_state_=="timeout_unconfirmed"?3:0;break;
      case 8:value=(fresh?1:0)|(modbus_config_.rtu?2:0)|(modbus_config_.tcp?4:0);break;
    }return 0;
  }
  if(!fresh)return 0x0b;
  if(table==Table::COIL){value=address==0?raw_control_.ac_power:address==1?raw_control_.quiet_mode:raw_control_.display_status;return 0;}
  if(table==Table::HOLDING){switch(address){
    case 0:value=raw_control_.set_point+16;break;
    case 1:value=mode_ycj(raw_control_.ac_mode);if(!value)return 4;break;
    case 2:value=fan_ycj(raw_control_.fan_mode);if(!value)return 4;break;
    case 3:value=raw_control_.lock_remote?4:1;break;
    case 4:value=((raw_control_.vertical_swing_mode==12 || raw_control_.vertical_swing_mode==14)?1:0)|(raw_control_.horizontal_swing_mode==7?2:0);break;
    case 5:value=raw_control_.fast_mode?1:raw_control_.sleep_mode?2:0;break;
    case 6:value=raw_control_.vertical_swing_mode;break;
    case 7:value=raw_control_.horizontal_swing_mode;break;
  }return 0;}
  if(!raw_sensors_valid_)return 0x0b;
  switch(address){
    case 0:value=raw_sensors_.room_temperature/2;break; // Factory resolution 1 C: floor .5 C.
    case 1:value=raw_sensors_.error_status;break; // Native hOn code; equivalence of nonzero YCJ codes unverified.
    case 2:value=0;break;
    case 3:value=uint16_t(raw_sensors_.room_temperature)*5;break;
  }return 0;
}
uint8_t Portal::write(const Change *changes,size_t count) {
  haier_bridge::Plan plan{};
  uint8_t error=haier_bridge::plan(changes,count,raw_control_,plan);if(error)return error;
  if(!seen_status_ || !raw_control_valid_ || !climate_->valid_connection() || uint32_t(millis()-last_status_)>10000)return 0x0b;
  if(test_pending_)return 6;
  auto next=plan.next;uint16_t mask=plan.mask;
  // Keep power unchanged when modifying registers, including mode while off.
  mask|=1;
  modbus_expected_=next;modbus_mask_=mask;modbus_pending_=true;
  desired_mode_=desired_fan_=desired_swing_=desired_preset_="";desired_target_set_=false;
  desired_quiet_=desired_display_=desired_vertical_=desired_horizontal_=-1;
  request_id_=String("mb-")+String(++modbus_commands_);mqtt_state_changed_(true);test_pending_=true;test_frame_=status_count_;test_matches_=0;
  test_started_=millis();test_state_="pending";control_window_=true;
  hon_()->set_control_method(haier::HonControlMethod::SET_GROUP_PARAMETERS);
  // Existing hOn group encoder preserves bytes not changed by these native setters.
  hon_()->set_bridge_overrides(next,mask);
  auto call=climate_->make_call();bool use_call=false;
  if(mask&2){call.set_target_temperature(next.set_point+16);use_call=true;}
  if(mask&8){const char *names[]={"","LOW","MEDIUM","HIGH","AUTO"};call.set_fan_mode(names[fan_ycj(next.fan_mode)]);use_call=true;}
  if(mask&512){call.set_preset(next.fast_mode?"BOOST":next.sleep_mode?"SLEEP":"NONE");use_call=true;}
  if(mask&32)hon_()->set_vertical_airflow(static_cast<haier::hon_protocol::VerticalSwingMode>(next.vertical_swing_mode));
  if(mask&64)hon_()->set_horizontal_airflow(static_cast<haier::hon_protocol::HorizontalSwingMode>(next.horizontal_swing_mode));
  if(mask&128)hon_()->set_quiet_mode_state(next.quiet_mode);
  if(mask&256)climate_->set_display_state(next.display_status);
  if(use_call)call.perform();
  return 0;
}
bool Portal::modbus_matches_() const {
  if(!raw_control_valid_)return false;
  const auto &a=raw_control_;const auto &b=modbus_expected_;uint16_t m=modbus_mask_;
  return (!(m&1)||a.ac_power==b.ac_power) && (!(m&2)||(a.set_point==b.set_point && !a.half_degree)) &&
    (!(m&4)||a.ac_mode==b.ac_mode) && (!(m&8)||a.fan_mode==b.fan_mode) && (!(m&16)||a.lock_remote==b.lock_remote) &&
    (!(m&32)||a.vertical_swing_mode==b.vertical_swing_mode) && (!(m&64)||a.horizontal_swing_mode==b.horizontal_swing_mode) &&
    (!(m&128)||a.quiet_mode==b.quiet_mode) && (!(m&256)||a.display_status==b.display_status) &&
    (!(m&512)||(a.fast_mode==b.fast_mode && a.sleep_mode==b.sleep_mode));
}
void Portal::modbus_setup_(){
  modbus_pref_=global_preferences->make_preference<ModbusConfig>(0x484d4201,true);
  ModbusConfig saved{};
  if(modbus_pref_.load(&saved) && saved.magic==0x484d4201 && saved.unit>=1 && saved.unit<=247 && saved.rtu<=1 && saved.tcp<=1 &&
      (saved.baud==9600 || saved.baud==19200 || saved.baud==38400 || saved.baud==57600 || saved.baud==115200))modbus_config_=saved;
  rs485_->set_baud_rate(modbus_config_.baud);rs485_->load_settings();
}
String Portal::modbus_json_(){
  return String("{\"rtu\":")+(modbus_config_.rtu?"true":"false")+",\"tcp\":"+(modbus_config_.tcp?"true":"false")+
    ",\"unit\":"+String(modbus_config_.unit)+",\"baud\":"+String(modbus_config_.baud)+",\"tcp_port\":502}";
}
void Portal::modbus_web_(){
  web_.on("/modbus",HTTP_GET,[this](){if(test_auth_())send_page_(MODBUS_PAGE);});
  web_.on("/modbus/config",HTTP_GET,[this](){if(!test_auth_())return;web_.sendHeader("Cache-Control","no-store");web_.send(200,"application/json",modbus_json_());});
  web_.on("/modbus/config",HTTP_POST,[this](){
    if(!test_auth_())return;if(web_.arg("token")!=token_){web_.send(403,"text/plain","Invalid token");return;}
    if(test_pending_){web_.send(409,"text/plain","Wait for pending command");return;}
    for(unsigned i=0;i<web_.args();++i){String k=web_.argName(i);if(k!="token" && k!="rtu" && k!="tcp" && k!="unit" && k!="baud"){web_.send(400,"text/plain","Unknown setting");return;}}
    String r=web_.arg("rtu"),t=web_.arg("tcp"),u=web_.arg("unit"),b=web_.arg("baud");
    auto decimal=[](const String &s){if(!s.length() || s.length()>6)return false;for(unsigned i=0;i<s.length();++i)if(s[i]<'0'||s[i]>'9')return false;return true;};
    long unit=u.toInt(),baud=b.toInt();
    if((r!="0"&&r!="1")||(t!="0"&&t!="1")||!decimal(u)||unit<1||unit>247||!decimal(b)||
       (baud!=9600&&baud!=19200&&baud!=38400&&baud!=57600&&baud!=115200)) {web_.send(400,"text/plain","Invalid settings");return;}
    auto next=modbus_config_;next.rtu=r=="1";next.tcp=t=="1";next.unit=unit;next.baud=baud;
    if(!modbus_pref_.save(&next)||!global_preferences->sync()){web_.send(503,"text/plain","Save failed");return;}
    modbus_config_=next;modbus_dirty_=true;web_.send(200,"application/json",modbus_json_());
  });
}
void Portal::modbus_loop_(){
  if(modbus_dirty_){
    for(auto &s:modbus_clients_){s.client.stop();s.frame.clear();}
    if(modbus_listening_){modbus_server_.end();modbus_listening_=false;}
    rs485_->set_baud_rate(modbus_config_.baud);rs485_->load_settings();rtu_used_=0;modbus_dirty_=false;
  }
  bool serve=modbus_config_.tcp && WiFi.status()==WL_CONNECTED;
  if(serve && !modbus_listening_){modbus_server_.begin();modbus_listening_=true;}
  if(!serve && modbus_listening_){for(auto &s:modbus_clients_){s.client.stop();s.frame.clear();}modbus_server_.end();modbus_listening_=false;}
  if(serve){
    WiFiClient incoming=modbus_server_.accept();
    if(incoming){bool assigned=false;if(incoming.localIP()==WiFi.localIP())for(auto &s:modbus_clients_)if(!s.client.connected()){
      s.client.stop();s.client=incoming;s.client.setTimeout(100);s.frame.clear();s.last_ms=millis();assigned=true;break;}
      if(!assigned)incoming.stop();}
    for(auto &s:modbus_clients_){
      if(!s.client.connected()){s.client.stop();s.frame.clear();continue;}
      if(uint32_t(millis()-s.last_ms)>(s.frame.used?2000:30000)){s.client.stop();s.frame.clear();continue;}
      for(unsigned budget=0;budget<260 && s.client.available();++budget){
        int v=s.client.read();if(v<0)break;s.last_ms=millis();int result=s.frame.push(v);
        if(result<0){s.client.stop();s.frame.clear();break;}
        if(result==1){uint8_t out[260];size_t n=haier_bridge::tcp(*this,s.frame.bytes,s.frame.used,modbus_config_.unit,out);s.frame.clear();
          if(!n || s.client.write(out,n)!=n){s.client.stop();break;}}
      }
    }
  }
  // Recognized request lengths allow a buffered UART to recover coalesced frames.
  // Unknown function frames are finalized after a conservative silent interval.
  uint32_t silence=modbus_config_.baud>19200?1750:(35000000UL+modbus_config_.baud-1)/modbus_config_.baud;
  auto dispatch=[this](){uint8_t out[256];size_t n=haier_bridge::rtu(*this,rtu_bytes_,rtu_used_,modbus_config_.unit,out);
    if(n && modbus_config_.rtu){rs485_->write_array(out,n);rs485_->flush();}rtu_used_=0;};
  if(rtu_used_ && uint32_t(micros()-rtu_last_us_)>silence && !rs485_->available()){if(modbus_config_.rtu)dispatch();else rtu_used_=0;}
  for(unsigned budget=0;budget<512 && rs485_->available();++budget){uint8_t v;if(!rs485_->read_byte(&v))break;
    if(!modbus_config_.rtu){rtu_used_=0;continue;}
    if(rtu_used_ && uint32_t(micros()-rtu_last_us_)>100000)rtu_used_=0;
    rtu_last_us_=micros();if(rtu_used_>=sizeof(rtu_bytes_)){rtu_used_=0;continue;}rtu_bytes_[rtu_used_++]=v;
    if(rtu_used_>=2){uint8_t fc=rtu_bytes_[1];size_t wanted=(fc==1||fc==3||fc==4||fc==5||fc==6)?8:
       ((fc==15||fc==16)&&rtu_used_>=7)?size_t(9+rtu_bytes_[6]):0;
      if(wanted>256){rtu_used_=0;continue;}
      if(wanted && rtu_used_==wanted)dispatch();
    }
  }
}
}} // namespace
