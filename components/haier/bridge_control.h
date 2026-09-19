// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include "hon_packet.h"
namespace esphome::haier::hon_protocol {
// Apply validated Modbus fields after the ordinary hOn group encoder.
// Other bytes remain those of the existing encoder; no new wire format.
inline void apply_bridge_control(HaierPacketControl &out, const HaierPacketControl &next, uint16_t mask) {
  if (mask & 1) out.ac_power = next.ac_power;
  if (mask & 2) { out.set_point = next.set_point; out.half_degree = next.half_degree; }
  if (mask & 4) out.ac_mode = next.ac_mode;
  if (mask & 8) out.fan_mode = next.fan_mode;
  if (mask & 16) out.lock_remote = next.lock_remote;
  if (mask & 32) out.vertical_swing_mode = next.vertical_swing_mode;
  if (mask & 64) out.horizontal_swing_mode = next.horizontal_swing_mode;
  if (mask & 128) out.quiet_mode = next.quiet_mode;
  if (mask & 256) out.display_status = next.display_status;
  if (mask & 512) { out.fast_mode = next.fast_mode; out.sleep_mode = next.sleep_mode; out.ten_degree = 0; }
  if (!out.ac_power || out.ac_mode == 6) { out.quiet_mode = 0; out.fast_mode = 0; out.sleep_mode = 0; }
}
}
