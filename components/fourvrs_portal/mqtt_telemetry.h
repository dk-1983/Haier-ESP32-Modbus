// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace esphome::fourvrs_portal {
inline bool telemetry_due(uint32_t now, uint32_t sent, bool reconnect, bool new_frame) {
  return !sent || reconnect || uint32_t(now-sent)>=5000 || (new_frame && uint32_t(now-sent)>=250);
}
inline bool telemetry_fault_valid(bool seen, bool connected, uint32_t age, bool sensors) {
  return seen && connected && age<30000 && sensors;
}
}
