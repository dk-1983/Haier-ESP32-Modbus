// SPDX-License-Identifier: MIT
#include "components/fourvrs_portal/register_model.h"
#include "components/haier/bridge_control.h"
#include "components/fourvrs_portal/mqtt_model.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <random>
using namespace haier_bridge;
using Control=esphome::haier::hon_protocol::HaierPacketControl;
struct Fake:Backend {
  Control state{};int writes=0;bool stale=false,busy=false;
  Fake(){state.ac_power=1;state.ac_mode=1;state.fan_mode=5;state.set_point=6;state.vertical_swing_mode=6;state.display_status=1;}
  uint8_t read(Table t,uint16_t a,uint16_t &v) override {
    if(a>(t==Table::COIL?2:t==Table::HOLDING?7:8))return 2;if(stale)return 11;
    v=t==Table::COIL?state.ac_power:a==0?state.set_point+16:a==1?mode_ycj(state.ac_mode):a==2?fan_ycj(state.fan_mode):0;return 0;
  }
  uint8_t write(const Change *c,size_t n) override {
    Plan out;uint8_t e=plan(c,n,state,out);if(e)return e;if(stale)return 11;if(busy)return 6;
    state=out.next;++writes;return 0;
  }
};
std::vector<uint8_t> run(Fake &b,std::initializer_list<uint8_t> in){uint8_t out[253]{};size_t n=pdu(b,in.begin(),in.size(),out);return {out,out+n};}
void eq(std::vector<uint8_t> a,std::initializer_list<uint8_t> b){assert(a==std::vector<uint8_t>(b));}
int main(){
  Fake b;
  eq(run(b,{3,0,0,0,3}),{3,6,0,22,0,1,0,4});
  eq(run(b,{1,0,0,0,1}),{1,1,1});
  eq(run(b,{4,0,2,0,1}),{4,2,0,4}); // Fake read function intentionally distinct from holding map.
  eq(run(b,{6,0,0,0,24}),{6,0,0,0,24});assert(b.state.set_point==8);
  eq(run(b,{5,0,0,0,0}),{5,0,0,0,0});assert(!b.state.ac_power);
  eq(run(b,{5,0,0,0xff,0}),{5,0,0,0xff,0});assert(b.state.ac_power);
  eq(run(b,{15,0,0,0,3,1,5}),{15,0,0,0,3});assert(b.state.ac_power && !b.state.quiet_mode && b.state.display_status);
  eq(run(b,{16,0,0,0,3,6,0,22,0,2,0,3}),{16,0,0,0,3});assert(b.state.ac_mode==4 && b.state.fan_mode==1);
  int before=b.writes;auto old=b.state;
  eq(run(b,{16,0,0,0,3,6,0,25,0,9,0,3}),{0x90,3});assert(b.writes==before && b.state.set_point==old.set_point);
  eq(run(b,{6,0,0,0,31}),{0x86,3});
  eq(run(b,{5,0,0,0,1}),{0x85,3});
  eq(run(b,{3,0xff,0xff,0,2}),{0x83,2});
  eq(run(b,{3,0,0,0,0}),{0x83,3});
  eq(run(b,{3,0,0,0,126}),{0x83,3});
  eq(run(b,{3,0,7,0,2}),{0x83,2});
  eq(run(b,{2,0,0,0,1}),{0x82,1});
  eq(run(b,{16,0,0,0,2,4,0,22}),{0x90,3});
  eq(run(b,{15,0,0,0,3,2,1,1}),{0x8f,3});
  b.stale=true;eq(run(b,{3,0,0,0,1}),{0x83,11});eq(run(b,{6,0,0,0,22}),{0x86,11});b.stale=false;
  b.busy=true;eq(run(b,{6,0,0,0,22}),{0x86,6});b.busy=false;
  Control initial{};initial.ac_mode=1;initial.fan_mode=5;initial.set_point=6;initial.ac_power=1;
  Plan p;
  for(int m=1;m<=5;++m){Change c{Table::HOLDING,1,uint16_t(m)};assert(plan(&c,1,initial,p)==0);assert(mode_ycj(p.next.ac_mode)==m);}
  for(int f=1;f<=4;++f){Change c{Table::HOLDING,2,uint16_t(f)};assert(plan(&c,1,initial,p)==0);assert(fan_ycj(p.next.fan_mode)==f);}
  for(int lock=1;lock<=4;++lock){Change c{Table::HOLDING,3,uint16_t(lock)};assert(plan(&c,1,initial,p)==0);assert(p.next.lock_remote==(lock==4));}
  initial.ac_power=0;Change heat{Table::HOLDING,1,2};assert(!plan(&heat,1,initial,p));assert(!p.next.ac_power && p.next.ac_mode==4);
  initial.ac_power=1;
  for(int v:{1,2,3,4,6,8}){Change c{Table::HOLDING,6,uint16_t(v)};assert(!plan(&c,1,initial,p));assert(p.next.vertical_swing_mode==v);}
  for(int v:{0,3,4,5,6}){Change c{Table::HOLDING,7,uint16_t(v)};assert(!plan(&c,1,initial,p));assert(p.next.horizontal_swing_mode==v);}
  Change conflict[]={{Table::HOLDING,4,3},{Table::HOLDING,6,6}};assert(plan(conflict,2,initial,p)==3);
  initial.fast_mode=1;Change quiet{Table::COIL,1,1};assert(plan(&quiet,1,initial,p)==3);initial.fast_mode=0;
  initial.ac_mode=6;assert(plan(&quiet,1,initial,p)==3);
  Change fan{Table::HOLDING,2,4};assert(plan(&fan,1,initial,p)==3);
  // Final group fields must follow a combined write even when old mode was FAN.
  initial.ac_mode=6;initial.fan_mode=3;initial.ac_power=1;
  Change combined[]={{Table::HOLDING,1,1},{Table::HOLDING,2,4}};
  assert(!plan(combined,2,initial,p));
  Control encoded=initial;encoded.lock_remote=1;encoded.display_status=1;
  esphome::haier::hon_protocol::apply_bridge_control(encoded,p.next,p.mask);
  assert(encoded.ac_mode==1 && encoded.fan_mode==5 && encoded.lock_remote && encoded.display_status);
  initial.ac_power=0;assert(!plan(&heat,1,initial,p));encoded=initial;
  esphome::haier::hon_protocol::apply_bridge_control(encoded,p.next,p.mask);
  assert(!encoded.ac_power && encoded.ac_mode==4);
  assert(crc16(reinterpret_cast<const uint8_t *>("123456789"),9)==0x4b37);
  uint8_t frame[256]={1,3,0,0,0,1},out[260]{};uint16_t crc=crc16(frame,6);frame[6]=crc;frame[7]=crc>>8;
  assert(rtu(b,frame,8,1,out)==7 && crc16(out,7)==0);
  frame[7]^=1;assert(rtu(b,frame,8,1,out)==0);frame[7]^=1;assert(rtu(b,frame,8,2,out)==0);
  frame[0]=0;crc=crc16(frame,6);frame[6]=crc;frame[7]=crc>>8;before=b.writes;assert(!rtu(b,frame,8,1,out)&&b.writes==before);
  frame[1]=6;frame[5]=22;crc=crc16(frame,6);frame[6]=crc;frame[7]=crc>>8;assert(!rtu(b,frame,8,1,out)&&b.writes==before+1);
  uint8_t adu[]={0x12,0x34,0,0,0,6,1,3,0,0,0,1};
  assert(tcp(b,adu,sizeof(adu),1,out)==11 && out[0]==0x12 && out[1]==0x34 && word(out+4)==5 && out[10]==22);
  assert(tcp(b,adu,sizeof(adu),2,out)==9 && out[7]==0x83 && out[8]==11);
  adu[2]=1;assert(!tcp(b,adu,sizeof(adu),1,out));adu[2]=0;
  for(size_t split=0;split<=sizeof(adu);++split){TcpFrame f;int ready=0;
    for(size_t i=0;i<split;++i)ready+=f.push(adu[i]);
    for(size_t i=split;i<sizeof(adu);++i)ready+=f.push(adu[i]);assert(ready==1);f.clear();
    for(uint8_t v:adu)if(f.push(v)==1){ready++;f.clear();}assert(ready==2);
  }
  TcpFrame invalid;uint8_t huge[]={0,0,0,0,0xff,0xff};int status=0;for(auto v:huge)status=invalid.push(v);assert(status==-1);
  // Malformed requests and bounded response writes (guard bytes after maximum PDU).
  std::mt19937 gen(42);uint8_t input[270],guard[270];
  for(int trial=0;trial<50000;++trial){size_t n=gen()%270;for(size_t j=0;j<n;++j)input[j]=gen();std::memset(guard,0xA5,sizeof(guard));
    size_t got=pdu(b,input,n,guard);assert(got<=253);for(size_t j=253;j<sizeof(guard);++j)assert(guard[j]==0xA5);}
  Change cmd{};
  assert(!mqtt_command("target","22",cmd) && cmd.table==Table::HOLDING && cmd.address==0 && cmd.value==22);
  for(const char *bad:{"", "0", "31", "22x", "22.0", " 22", "22\n"})assert(mqtt_command("target",bad,cmd)==3);
  assert(!mqtt_command("power","ON",cmd) && cmd.table==Table::COIL && cmd.value==1);
  assert(mqtt_command("power","on",cmd)==3 && mqtt_command("unknown","ON",cmd)==2);
  assert(!mqtt_command("mode","HEAT",cmd) && cmd.address==1 && cmd.value==2);
  assert(!mqtt_command("fan","AUTO",cmd) && cmd.value==4);
  assert(!mqtt_command("lock","LOCK",cmd) && cmd.value==4);
  assert(!mqtt_command("swing","BOTH",cmd) && cmd.value==3);
  assert(!mqtt_command("preset","SLEEP",cmd) && cmd.value==2);
  assert(!mqtt_command("vertical_position","DOWN",cmd) && cmd.value==8);
  assert(!mqtt_command("horizontal_position","MAX_RIGHT",cmd) && cmd.value==6);
  assert(!mqtt_command("quiet","OFF",cmd) && cmd.address==1 && cmd.value==0);
  assert(!mqtt_command("display","ON",cmd) && cmd.address==2 && cmd.value==1);
  assert(mqtt_prefix("haier/ac-1") && !mqtt_prefix("haier/#") && !mqtt_prefix("haier//ac") && !mqtt_prefix("/haier"));
  assert(mqtt_host("mqtt.local") && mqtt_host("192.168.1.2") && !mqtt_host("mqtt://host") && !mqtt_host("host:1883"));
  const char *topic="haier/set/target";
  for(size_t split=0;split<2;++split){MqttAssembly a;
    assert(!a.feed(topic,strlen(topic),"22",split,0,2,false));
    assert(a.feed(nullptr,0,&"22"[split],2-split,split,2,false)==(split!=0));
    // A repeated offset-zero fragment without its topic is invalid, never a command.
  }
  MqttAssembly a;assert(a.feed(topic,strlen(topic),"22",2,0,2,true) && a.message.retained);
  assert(!a.feed(topic,strlen(topic),"2",1,0,200,false));
  assert(!a.feed(topic,strlen(topic),"2",1,0,2,false));assert(!a.feed(nullptr,0,"2",1,2,2,false));
  const char embedded_zero[]={'2',0};assert(!a.feed(topic,strlen(topic),embedded_zero,2,0,2,false));
  assert(a.feed(topic,strlen(topic),"22",2,0,2,false) && !strcmp(a.message.payload,"22"));
  std::puts("PASS: MQTT command mapping, validation, bounded fragmented messages and retained flag");
  std::puts("PASS: YCJ mapping, atomic writes, 7 function codes, exceptions, CRC/broadcast, TCP framing, 50000 malformed PDUs");
}
