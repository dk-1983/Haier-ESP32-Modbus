// SPDX-License-Identifier: MIT
#pragma once
#include "../haier/hon_packet.h"
#include <cstddef>
#include <cstring>
namespace esphome::fourvrs_portal {
inline bool copy_status_sensors(haier::hon_protocol::HaierPacketSensors &out,
                                const char *data, size_t size) {
  constexpr size_t offset = 2 + sizeof(haier::hon_protocol::HaierPacketControl);
  static_assert(sizeof(out) == 18, "Review hOn sensor layout before changing the decoder");
  // The four protocol extension bytes are not members of HaierPacketSensors.
  if (data == nullptr || size < offset + sizeof(out) + 4) return false;
  std::memcpy(&out, data + offset, sizeof(out));
  return true;
}
}
