// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
#include "MqttPage.h"
namespace esphome::fourvrs_portal {
void mqtt_worker_task(void *arg);
static bool valid_config(const MqttConfig &c) {
  if(c.magic!=0x484d5101 || c.enabled>1 || c.discovery_disabled>1 || !c.port)return false;
  if(!memchr(c.host,0,sizeof(c.host)) || !memchr(c.username,0,sizeof(c.username)) || !memchr(c.password,0,sizeof(c.password)) || !memchr(c.prefix,0,sizeof(c.prefix)))return false;
  return haier_bridge::mqtt_prefix(c.prefix) && (!c.host[0]? !c.enabled : haier_bridge::mqtt_host(c.host));
}
void Portal::mqtt_setup_() {
  mqtt_pref_=global_preferences->make_preference<MqttConfig>(0x484d5101,true);
  hostname_.toCharArray(mqtt_config_.prefix,sizeof(mqtt_config_.prefix));
  hostname_.toCharArray(mqtt_runtime_.client_id,sizeof(mqtt_runtime_.client_id));
  MqttConfig saved;if(mqtt_pref_.load(&saved) && valid_config(saved))mqtt_config_=saved;
  auto &r=mqtt_runtime_;
  r.config_queue=xQueueCreate(1,sizeof(MqttConfig));r.rx_queue=xQueueCreate(8,sizeof(haier_bridge::MqttMessage));r.tx_queue=xQueueCreate(4,sizeof(MqttPublish));
  if(!r.config_queue || !r.rx_queue || !r.tx_queue){r.last_error=-5;return;}
  if(xTaskCreate(mqtt_worker_task,"haier_mqtt_io",8192,&r,1,&r.task)!=pdPASS){r.task=nullptr;r.last_error=-6;return;}
  xQueueOverwrite(r.config_queue,&mqtt_config_);
}
void Portal::mqtt_state_changed_(bool pending) {
  mqtt_runtime_.state_pending=pending;++mqtt_runtime_.state_epoch;mqtt_last_publish_=0;
}
String Portal::mqtt_json_() {
  return String("{\"discovery\":")+(!mqtt_config_.discovery_disabled?"true":"false")+",\"discovery_sent\":"+String(mqtt_runtime_.discovery_acked.load())+",\"enabled\":"+(mqtt_config_.enabled?"true":"false")+",\"host\":"+json_string_(mqtt_config_.host)+
    ",\"port\":"+String(mqtt_config_.port)+",\"username\":"+json_string_(mqtt_config_.username)+",\"prefix\":"+json_string_(mqtt_config_.prefix)+
    ",\"password_set\":"+(mqtt_config_.password[0]?"true":"false")+",\"connected\":"+(mqtt_runtime_.connected?"true":"false")+
    ",\"worker_ready\":"+(mqtt_runtime_.task?"true":"false")+",\"connections\":"+String(mqtt_runtime_.connections.load())+
    ",\"dropped\":"+String(mqtt_runtime_.dropped.load())+",\"last_error\":"+String(mqtt_runtime_.last_error.load())+"}";
}
void Portal::mqtt_web_() {
  web_.on("/mqtt",HTTP_GET,[this](){if(test_auth_())send_page_(MQTT_PAGE);});
  web_.on("/mqtt/config",HTTP_GET,[this](){if(!test_auth_())return;web_.sendHeader("Cache-Control","no-store");web_.send(200,"application/json",mqtt_json_());});
  web_.on("/mqtt/config",HTTP_POST,[this](){
    if(!test_auth_())return;if(web_.arg("token")!=token_){web_.send(403,"text/plain","Invalid token");return;}
    if(mqtt_runtime_.applying){web_.send(409,"text/plain","MQTT settings are being applied");return;}
    if(!mqtt_runtime_.task){web_.send(503,"text/plain","MQTT worker unavailable");return;}
    for(unsigned i=0;i<web_.args();++i){String k=web_.argName(i);if(k!="token"&&k!="enabled"&&k!="host"&&k!="port"&&k!="username"&&k!="password"&&k!="clear_password"&&k!="prefix"&&k!="discovery"){web_.send(400,"text/plain","Unknown setting");return;}}
    String en=web_.arg("enabled"),host=web_.arg("host"),port=web_.arg("port"),user=web_.arg("username"),pass=web_.arg("password"),clear=web_.arg("clear_password"),prefix=web_.arg("prefix");
    bool digits=port.length()>0 && port.length()<=5;for(unsigned i=0;i<port.length();++i)digits&=port[i]>='0'&&port[i]<='9';
    bool lengths=host.length()<128 && user.length()<65 && pass.length()<129 && prefix.length()<65;
    for(const String *v:{&host,&user,&pass,&prefix})lengths&=strlen(v->c_str())==v->length();
    if((en!="0"&&en!="1")||(clear!="0"&&clear!="1")||(clear=="1"&&pass.length())||!digits||port.toInt()<1||port.toInt()>65535||!lengths){web_.send(400,"text/plain","Invalid MQTT settings");return;}
    MqttConfig next=mqtt_config_;
    if(web_.hasArg("discovery")){String d=web_.arg("discovery");if(d!="0"&&d!="1"){web_.send(400,"text/plain","Invalid discovery setting");return;}next.discovery_disabled=d=="0";}
    next.enabled=en=="1";next.port=port.toInt();host.toCharArray(next.host,sizeof(next.host));user.toCharArray(next.username,sizeof(next.username));prefix.toCharArray(next.prefix,sizeof(next.prefix));
    if(clear=="1")memset(next.password,0,sizeof(next.password));else if(pass.length()){memset(next.password,0,sizeof(next.password));pass.toCharArray(next.password,sizeof(next.password));}
    if(!valid_config(next)){web_.send(400,"text/plain","Invalid host or topic prefix");return;}
    if(!mqtt_pref_.save(&next)||!global_preferences->sync()){web_.send(503,"text/plain","Save failed");return;}
    mqtt_config_=next;mqtt_runtime_.applying=true;xQueueOverwrite(mqtt_runtime_.config_queue,&next);web_.send(200,"application/json",mqtt_json_());
  });
}
bool Portal::mqtt_publish_(const char *suffix,const String &body) {
  if(!mqtt_config_.enabled || !mqtt_runtime_.task || mqtt_runtime_.applying || !mqtt_runtime_.connected || body.length()>=sizeof(MqttPublish::payload))return false;
  MqttPublish publish{};publish.state=!strcmp(suffix,"state");publish.epoch=mqtt_runtime_.state_epoch.load();snprintf(publish.topic,sizeof(publish.topic),"%s/%s",mqtt_config_.prefix,suffix);body.toCharArray(publish.payload,sizeof(publish.payload));
  if(xQueueSend(mqtt_runtime_.tx_queue,&publish,0)!=pdTRUE){++mqtt_runtime_.dropped;return false;}return true;
}
void Portal::mqtt_loop_() {
  if(!mqtt_runtime_.task || mqtt_runtime_.applying)return;
  // Report the prior MQTT command before accepting another interface's next command.
  if(mqtt_pending_id_.length() && !test_pending_){
    mqtt_publish_("result",String("{\"request_id\":")+json_string_(mqtt_pending_id_)+",\"status\":"+json_string_(test_state_)+"}");mqtt_pending_id_="";
  }
  haier_bridge::MqttMessage message;
  for(unsigned budget=0;budget<4 && xQueueReceive(mqtt_runtime_.rx_queue,&message,0)==pdTRUE;++budget){
    if(!mqtt_config_.enabled || !mqtt_runtime_.connected)continue;
    String prefix=String(mqtt_config_.prefix)+"/set/";
    if(strncmp(message.topic,prefix.c_str(),prefix.length()))continue;
    const char *field=message.topic+prefix.length();
    uint8_t error=0;const char *reason=nullptr;haier_bridge::Change changes[3]{};size_t count=0;
    if(message.retained)reason="retained_command_rejected";
    else if(uint32_t(millis()-message.received_ms)>1000)reason="expired_command";
    else error=haier_bridge::mqtt_changes(field,message.payload,changes,count);
    if(!reason && !error)error=write(changes,count);
    if(reason || error){mqtt_last_publish_=0;mqtt_publish_("result",String("{\"status\":\"rejected\",\"field\":")+json_string_(field)+",\"error\":"+String(error)+",\"reason\":"+json_string_(reason?reason:"register_validation")+"}");continue;}
    request_id_=String("mqtt-")+String(++mqtt_command_count_);mqtt_pending_id_=request_id_;
    mqtt_publish_("result",String("{\"status\":\"accepted\",\"request_id\":")+json_string_(request_id_)+",\"field\":"+json_string_(field)+"}");
  }
  uint32_t now=millis(),connections=mqtt_runtime_.connections.load();
  if(!mqtt_runtime_.connected){mqtt_last_publish_=0;return;}
  if(test_pending_)return;
  if(!mqtt_last_publish_ || connections!=mqtt_seen_connections_ || uint32_t(now-mqtt_last_publish_)>=5000){
    if(mqtt_publish_("state",climate_status_(false))){mqtt_last_publish_=now;mqtt_seen_connections_=connections;}
  }
}
}
