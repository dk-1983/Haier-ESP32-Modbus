// SPDX-License-Identifier: MIT
#pragma once
#include "modbus_core.h"
#include <cstring>
namespace haier_bridge {
inline bool mqtt_prefix(const char *s) {
  size_t n=std::strlen(s); if(!n || n>64 || s[0]=='/' || s[n-1]=='/') return false;
  for(size_t i=0;i<n;++i) {
    char c=s[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='/'))return false;
    if(c=='/' && i && s[i-1]=='/')return false;
  }
  return true;
}
inline bool mqtt_host(const char *s) {
  size_t n=std::strlen(s);if(!n || n>127)return false;
  for(size_t i=0;i<n;++i){char c=s[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return false;}
  return true;
}
inline int mqtt_enum(const char *s,const char *const *names,size_t count) {
  for(size_t i=0;i<count;++i)if(!std::strcmp(s,names[i]))return i;return -1;
}
// MQTT maps to the same validated register backend; it never drives hOn itself.
inline uint8_t mqtt_command(const char *field,const char *value,Change &out) {
  const char *const power[]={"OFF","ON"};
  const char *const modes[]={"COOL","HEAT","DRY","FAN_ONLY","AUTO"};
  const char *const fans[]={"LOW","MEDIUM","HIGH","AUTO"};
  const char *const swing[]={"OFF","VERTICAL","HORIZONTAL","BOTH"};
  const char *const presets[]={"NONE","BOOST","SLEEP"};
  const char *const locks[]={"UNLOCK","LOCK"};
  const char *const vertical[]={"HEALTH_UP","MAX_UP","HEALTH_DOWN","UP","CENTER","DOWN"};
  const uint16_t v_codes[]={1,2,3,4,6,8};
  const char *const horizontal[]={"CENTER","MAX_LEFT","LEFT","RIGHT","MAX_RIGHT"};
  const uint16_t h_codes[]={0,3,4,5,6};
  int v=-1;uint16_t address=0;Table table=Table::HOLDING;
  if(!std::strcmp(field,"power") || !std::strcmp(field,"quiet") || !std::strcmp(field,"display")) {
    v=mqtt_enum(value,power,2);table=Table::COIL;address=!std::strcmp(field,"power")?0:!std::strcmp(field,"quiet")?1:2;
  } else if(!std::strcmp(field,"target")) {
    if(std::strlen(value)!=2 || value[0]<'0' || value[0]>'9' || value[1]<'0' || value[1]>'9')return 3;
    v=(value[0]-'0')*10+value[1]-'0';if(v<16 || v>30)return 3;
  } else if(!std::strcmp(field,"mode")) {v=mqtt_enum(value,modes,5);if(v>=0)++v;address=1;}
  else if(!std::strcmp(field,"fan")) {v=mqtt_enum(value,fans,4);if(v>=0)++v;address=2;}
  else if(!std::strcmp(field,"lock")) {v=mqtt_enum(value,locks,2);if(v>=0)v=v?4:1;address=3;}
  else if(!std::strcmp(field,"swing")) {v=mqtt_enum(value,swing,4);address=4;}
  else if(!std::strcmp(field,"preset")) {v=mqtt_enum(value,presets,3);address=5;}
  else if(!std::strcmp(field,"vertical_position")) {v=mqtt_enum(value,vertical,6);if(v>=0)v=v_codes[v];address=6;}
  else if(!std::strcmp(field,"horizontal_position")) {v=mqtt_enum(value,horizontal,5);if(v>=0)v=h_codes[v];address=7;}
  else return 2;
  if(v<0)return 3;out={table,address,uint16_t(v)};return 0;
}
struct MqttMessage {char topic[128]{};char payload[96]{};bool retained{false};uint32_t received_ms{0};};
struct MqttAssembly {
  MqttMessage message{};size_t used{0},expected{0};bool active{false};
  void clear(){active=false;used=expected=0;}
  // ESP-MQTT may split one PUBLISH into several callbacks. No truncated command is accepted.
  bool feed(const char *topic,size_t topic_size,const char *data,size_t length,size_t offset,size_t total,bool retained) {
    if(offset==0){clear();if(!topic || !topic_size || topic_size>=sizeof(message.topic) || !total || total>=sizeof(message.payload))return false;
      if(std::memchr(topic,0,topic_size))return false;
      message={};std::memcpy(message.topic,topic,topic_size);message.retained=retained;expected=total;active=true;
    }
    if(!active || total!=expected || offset!=used || length>expected-used || (length && (!data || std::memchr(data,0,length)))){clear();return false;}
    if(length)std::memcpy(message.payload+used,data,length);used+=length;
    if(used==expected){message.payload[used]=0;active=false;return true;}return false;
  }
};
}
