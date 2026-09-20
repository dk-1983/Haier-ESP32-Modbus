// SPDX-License-Identifier: MIT
// Standalone hOn bench peer. Wire layout checked against HaierProtocol 0.9.31
// and this project's hOn configuration; not a complete Haier appliance emulator.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace hon_bench {
static const uint8_t MAX_PAYLOAD = 64;
struct Message {
  uint8_t type = 0, size = 0;
  bool crc = false;
  uint8_t data[MAX_PAYLOAD] = {};
};
inline uint16_t crcByte(uint16_t crc, uint8_t b) {
  crc ^= b;
  for (uint8_t i = 0; i < 8; ++i)
    crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
  return crc;
}
template<class Output> void escaped(Output &out, uint8_t b) {
  out.write(b);
  if (b == 0xFF) out.write(uint8_t(0x55));
}
template<class Output> void send(Output &out, const Message &m) {
  if (m.size > MAX_PAYLOAD) return;
  out.write(uint8_t(0xFF)); out.write(uint8_t(0xFF));
  uint8_t sum = 0;
  uint16_t crc = 0;
  for (uint8_t i = 0; i < 8 + m.size; ++i) {
    uint8_t b = 0;
    if (i == 0) b = 8 + m.size;
    else if (i == 1) b = m.crc ? 0x40 : 0;
    else if (i == 7) b = m.type;
    else if (i >= 8) b = m.data[i - 8];
    sum += b;
    if (b == 0xFF) sum += 0x55; // hOn checksum includes stuffing bytes
    crc = crcByte(crc, b);      // CRC excludes stuffing and checksum
    escaped(out, b);
  }
  escaped(out, sum);
  if (m.crc) { escaped(out, uint8_t(crc >> 8)); escaped(out, uint8_t(crc)); }
}

class Parser {
 public:
  uint32_t bytes = 0, good = 0, bad = 0, timeouts = 0;
  void expire(uint32_t now) {
    if ((active_ || sync_) && uint32_t(now - last_) > 100) {
      ++timeouts; reset();
    }
  }
  bool feed(uint8_t b, uint32_t now, Message &m) {
    expire(now); last_ = now; ++bytes;
    if (!active_) {
      if (b == 0xFF && sync_) { active_ = true; sync_ = false; }
      else sync_ = b == 0xFF;
      return false;
    }
    if (escape_) {
      escape_ = false;
      if (b == 0xFF) { ++bad; count_ = 0; return false; } // new preamble
      if (b != 0x55) { ++bad; reset(); return false; }
      b = 0xFF;
    } else if (b == 0xFF) { escape_ = true; return false; }
    if (count_ >= sizeof(buf_)) { ++bad; reset(); return false; }
    buf_[count_++] = b;
    if (count_ == 1 && (b < 8 || b > 8 + MAX_PAYLOAD)) {
      ++bad; reset(); return false;
    }
    if (count_ < 2) return false;
    const uint8_t len = buf_[0];
    const bool has_crc = (buf_[1] & 0x40) != 0;
    if (count_ != len + 1 + (has_crc ? 2 : 0)) return false;
    uint8_t sum = 0; uint16_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
      sum += buf_[i]; if (buf_[i] == 0xFF) sum += 0x55;
      crc = crcByte(crc, buf_[i]);
    }
    const bool valid = sum == buf_[len] && (!has_crc ||
        crc == uint16_t((uint16_t(buf_[len + 1]) << 8) | buf_[len + 2]));
    if (valid) {
      m.type = buf_[7]; m.size = len - 8; m.crc = has_crc;
      memcpy(m.data, buf_ + 8, m.size); ++good;
    } else ++bad;
    reset(); return valid;
  }
 private:
  uint8_t buf_[8 + MAX_PAYLOAD + 3] = {}, count_ = 0;
  uint32_t last_ = 0;
  bool active_ = false, sync_ = false, escape_ = false;
  void reset() { count_ = 0; active_ = sync_ = escape_ = false; }
};

class Appliance {
 public:
  // 10 control bytes; 18 sensor bytes + 4 configured extra sensor bytes.
  uint8_t control[10] = {8, 6, 0x25, 0, 2, 0, 0, 0, 0, 0};
  uint32_t commands = 0, unsupported = 0;
  bool reply(const Message &in, Message &out) {
    out = Message(); out.crc = in.crc;
    switch (in.type) {
      case 0x61:
        out.type = 0x62; out.size = 38;
        memcpy(out.data, "SIM1.0", 6);
        memcpy(out.data + 8, "BENCH01", 7);
        memcpy(out.data + 19, "AVR5V", 5);
        memcpy(out.data + 28, "SIMULATR", 8);
        out.data[37] = 0x04; // advertise CRC, no optional interactive roles
        return true;
      case 0x70:
        out.type = 0x71; out.size = 16;
        memcpy(out.data, "BENCH-NOT-HAIER", 14); return true;
      case 0x73:
        out.type = 0x74; out.size = 10; return true; // no alarms
      case 0xFC:
        out.type = 0xFD; out.size = 2; return true;
      case 0xF7: out.type = 0x05; return true;
      case 0x05: return false; // never acknowledge an acknowledgement
      case 0x01: break;
      default: return reject(out);
    }
    if (in.size < 2) return reject(out);
    const uint16_t sub = (uint16_t(in.data[0]) << 8) | in.data[1];
    if (sub == 0x6001) {
      if (in.size != 12) return reject(out);
      memcpy(control, in.data + 2, 10); ++commands;
    } else if (sub == 0x4D01 || sub == 0x4DFE) {
      if (in.size != 2) return reject(out);
    } else if ((sub & 0xFF00) == 0x5D00) {
      if (in.size != 4 || in.data[2] != 0 || !setParameter(in.data[1], in.data[3]))
        return reject(out);
      ++commands;
    } else return reject(out);
    out.type = 0x02; out.size = sub == 0x4DFE ? 48 : 34;
    out.data[0] = sub == 0x4DFE ? 0x7D : 0x6D; out.data[1] = 1;
    memcpy(out.data + 2, control, 10);
    out.data[12] = 47; // synthetic 23.5 C room temperature
    out.data[13] = 45; // synthetic humidity
    out.data[14] = 84; // synthetic 20 C outdoors
    out.data[17] = 3;  // command source: communication module
    if (sub == 0x4DFE) {
      out.data[36] = 80; // indoor coil: 20 C
      out.data[37] = out.data[38] = out.data[39] = out.data[40] = 84;
      // All actuators/power/frequency remain zero; no thermal simulation.
    }
    return true;
  }
 private:
  bool reject(Message &out) { ++unsupported; out.type = 0x03; return true; }
  void setFlag(uint8_t index, uint8_t mask, uint8_t value) {
    control[index] = (control[index] & ~mask) | (value ? mask : 0);
  }
  bool setParameter(uint8_t id, uint8_t v) {
    switch (id) {
      case 1: setFlag(5, 1, v); break;
      case 2: if (v > 14) return false; control[0] = v; break;
      case 3: control[1] = (control[1] & 0xF0) | (v & 15); break;
      case 4: control[2] = (control[2] & 31) | ((v & 7) << 5); break;
      case 5: control[2] = (control[2] & 0xF8) | (v & 7); break;
      case 7: setFlag(4, 32, v); break;
      case 9: setFlag(4, 2, v); break;
      case 10: setFlag(4, 1, v); break;
      case 11: setFlag(5, 2, v); break;
      case 12: control[7] = (control[7] & 0xF8) | (v & 7); break;
      case 22: setFlag(5, 128, v); break;
      case 23: setFlag(5, 64, v); break;
      case 25: setFlag(5, 16, v); break;
      case 26: setFlag(5, 8, v); break;
      case 27: setFlag(5, 32, v); break;
      default: return false;
    }
    return true;
  }
};
} // namespace hon_bench
