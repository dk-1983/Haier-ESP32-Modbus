// SPDX-License-Identifier: MIT
#include "components/fourvrs_portal/status_sensors.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
int main() {
  struct Guarded { uint8_t before[8]; esphome::haier::hon_protocol::HaierPacketSensors sensors; uint8_t after[8]; } g;
  char frame[48]; for (unsigned i=0;i<sizeof(frame);++i) frame[i]=char(i+1);
  for (size_t n=0;n<=sizeof(frame);++n) {
    std::memset(&g,0xA5,sizeof(g));
    bool ok=esphome::fourvrs_portal::copy_status_sensors(g.sensors,frame,n);
    assert(ok == (n>=34));
    for(auto v:g.before) assert(v==0xA5);
    for(auto v:g.after) assert(v==0xA5);
    if(ok) assert(std::memcmp(&g.sensors,frame+12,18)==0);
    else for(auto v:reinterpret_cast<const uint8_t(&)[18]>(g.sensors)) assert(v==0xA5);
  }
  assert(!esphome::fourvrs_portal::copy_status_sensors(g.sensors,nullptr,34));
  std::puts("hOn sensor bounds: PASS");
}
