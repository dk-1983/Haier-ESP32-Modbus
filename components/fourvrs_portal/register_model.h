// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include "modbus_core.h"
#include "../haier/hon_packet.h"
namespace haier_bridge {
static const uint8_t MODES[] = {0,1,4,2,6,0}; // YCJ -> hOn COOL,HEAT,DRY,FAN,AUTO
static const uint8_t FANS[] = {0,3,2,1,5};
static uint16_t mode_ycj(uint8_t v){for(int i=1;i<=5;++i)if(MODES[i]==v)return i;return 0;}
static uint16_t fan_ycj(uint8_t v){for(int i=1;i<=4;++i)if(FANS[i]==v)return i;return 0;}
struct Plan { esphome::haier::hon_protocol::HaierPacketControl next{}; uint16_t mask{0}; };
inline uint8_t plan(const Change *changes,size_t count,const esphome::haier::hon_protocol::HaierPacketControl &current,Plan &output) {
  if(!count || count>8)return 3;
  // All validation precedes setters: FC16 must never partially apply.
  auto next=current;uint16_t mask=0;bool explicit_swing=false,position=false;
  for(size_t i=0;i<count;++i){auto c=changes[i];
    if(c.table==Table::COIL){if(c.address>2)return 2;if(c.value>1)return 3;
      if(c.address==0){next.ac_power=c.value;mask|=1;}
      if(c.address==1){next.quiet_mode=c.value;mask|=128;}
      if(c.address==2){next.display_status=c.value;mask|=256;}
    }else if(c.table==Table::HOLDING){switch(c.address){
      case 0:if(c.value<16 || c.value>30)return 3;next.set_point=c.value-16;next.half_degree=0;mask|=2;break;
      case 1:if(c.value<1 || c.value>5)return 3;next.ac_mode=MODES[c.value];mask|=4;break;
      case 2:if(c.value<1 || c.value>4)return 3;next.fan_mode=FANS[c.value];mask|=8;break;
      case 3:if(c.value<1 || c.value>4)return 3;next.lock_remote=c.value==4;mask|=16;break;
      case 4:if(c.value>3)return 3;explicit_swing=true;
        next.vertical_swing_mode=(c.value&1)?12:6;next.horizontal_swing_mode=(c.value&2)?7:0;mask|=96;break;
      case 5:if(c.value>2)return 3;next.fast_mode=c.value==1;next.sleep_mode=c.value==2;mask|=512;break;
      case 6:if(c.value!=1 && c.value!=2 && c.value!=3 && c.value!=4 && c.value!=6 && c.value!=8)return 3;
        next.vertical_swing_mode=c.value;position=true;mask|=32;break;
      case 7:if(c.value!=0 && c.value!=3 && c.value!=4 && c.value!=5 && c.value!=6)return 3;
        next.horizontal_swing_mode=c.value;position=true;mask|=64;break;
      default:return 2;
    }}else return 2;
  }
  if(explicit_swing && position)return 3; // Ambiguous simultaneous position + swing.
  if((mask&8) && next.ac_mode==6 && next.fan_mode==5)return 3;
  // Switching into FAN from AUTO fan chooses LOW, matching the local control constraints.
  if((mask&4) && next.ac_mode==6 && next.fan_mode==5){next.fan_mode=3;mask|=8;}
  if((mask&128) && next.quiet_mode && (!next.ac_power || next.ac_mode==6 || next.fast_mode))return 3;
  if((mask&512) && (next.fast_mode || next.sleep_mode) && (!next.ac_power || next.ac_mode==6))return 3;
  if((mask&512) && next.fast_mode && next.quiet_mode)return 3;
  output.next=next;output.mask=mask|1;return 0;
}
}
