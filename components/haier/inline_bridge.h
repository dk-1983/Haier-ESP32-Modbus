// SPDX-License-Identifier: MIT
// hOn inline transport. Factory-originated packets retain their wire representation.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace haier_inline {
constexpr size_t WIRE_MAX = 519;  // Maximum stuffed frame + one extra delimiter byte.
struct Frame {
  uint8_t wire[WIRE_MAX]{}, body[258]{};
  size_t wire_size{0}, size{0};
  bool escape{false}, short_preamble{false}, extra_preamble{false};
  uint32_t last{0};
  uint8_t type() const { return body[7]; }
  bool crc() const { return (body[1] & 0x40) != 0; }
  size_t payload_size() const { return body[0] - 8; }
  const uint8_t *payload() const { return body + 8; }
  void clear() { wire_size = size = 0; escape = short_preamble = extra_preamble = false; }
  // 0: partial, 1: complete valid frame, -1: raw/invalid data (forward unchanged).
  int put(uint8_t b, uint32_t now, bool allow_short = false) {
    last = now;
    if (wire_size >= WIRE_MAX) return -1;
    wire[wire_size++] = b;
    if (wire_size == 1) return b == 0xFF ? 0 : -1;
    if (wire_size == 2) {
      if (b == 0xFF) return 0;
      if (!allow_short || b < 8) return -1;
      short_preamble = true;
    }
    // Main RX captures also contain FF FF FF before an otherwise intact frame.
    // At this point FF FF FF 55 is the legitimate escaped length 255, not
    // an extra delimiter. Keep it unchanged. Accept at most one extra FF,
    // and do not normalize any bytes until checksum AND CRC have passed.
    if (allow_short && wire_size == 4 && size == 0 && escape && b != 0x55 && b >= 8) {
      extra_preamble = true;
      if (b == 0xFF) return 0;  // Extra delimiter followed by escaped length 255.
      escape = false;
    }
    if (escape) {
      escape = false;
      if (b != 0x55) return -1;
      b = 0xFF;
    } else if (b == 0xFF) { escape = true; return 0; }
    if (size >= sizeof(body)) return -1;
    body[size++] = b;
    // Nonstandard delimiters are accepted only with both integrity checks.
    if ((short_preamble || extra_preamble) && size == 2 && !crc()) return -1;
    if (size == 1 && b < 8) return -1;
    if (size < 2 || size < size_t(body[0]) + 1 + (crc() ? 2 : 0)) return 0;
    uint8_t sum = 0;
    uint16_t check = 0;
    for (size_t i = 0; i < body[0]; ++i) {
      sum += body[i];
      if (body[i] == 0xFF) sum += 0x55;
      check ^= body[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        check = (check >> 1) ^ ((check & 1) ? 0xA001 : 0);
    }
    bool valid = sum == body[body[0]] && (!crc() ||
        check == uint16_t((uint16_t(body[body[0] + 1]) << 8) | body[body[0] + 2]));
    if (!valid) return -1;
    if (short_preamble) {
      if (wire_size >= WIRE_MAX) return -1;
      memmove(wire + 1, wire, wire_size); wire[0] = 0xFF; ++wire_size;
    }
    if (extra_preamble) {
      memmove(wire, wire + 1, wire_size - 1); --wire_size;
    }
    return 1;
  }
};

struct Sink {
  virtual ~Sink() = default;
  virtual void to_main(const uint8_t *, size_t) = 0;
  virtual void to_factory(const uint8_t *, size_t) = 0;
  virtual void to_local(const uint8_t *, size_t) = 0;
  virtual void observe(const Frame &) = 0;
};

class Bridge {
 public:
  explicit Bridge(Sink &sink) : sink_(sink) {}
  struct Counters {
    uint32_t main_frames{0}, factory_frames{0}, local_requests{0}, local_replies{0};
    uint32_t invalid{0}, partial_timeouts{0}, transaction_timeouts{0}, overflows{0}, rejected{0};
    uint32_t factory_fallbacks{0};
    uint32_t recovered_preambles{0};
  } counters;
  uint8_t last_main_type{0}, last_main_flags{0}, last_main_address[5]{};
  uint8_t last_local_type{0}, last_local_flags{0};
  uint8_t version_flags{0}, version_address[5]{};
  uint32_t invalid_side[2]{}, local_timeouts{0}, pending_timeouts[2]{};
  uint8_t timeout_type[2]{}, invalid_source{0};
  uint8_t invalid_wire[64]{};
  size_t invalid_size{0};
  uint8_t error_context[128]{};
  size_t error_context_size{0};
  bool factory_present() const { return factory_seen_; }
  bool busy() const { return own_; }
  bool passthrough_only() const { return passthrough_; }
  const char *passthrough_reason() const { return passthrough_reason_; }
  const char *wait_reason(uint32_t now) const {
    if (passthrough_) return passthrough_reason_;
    if (own_) return "local_reply";
    if (pending_[0] || pending_[1]) return "factory_transaction";
    if (held_size_) return "held_factory_bytes";
    if (rx_[0].wire_size || rx_[1].wire_size) return "partial_frame";
    if (now - boot_ < 5000) return "startup";
    if (quarantine_ && now - fault_at_ < 2000) return "error_quarantine";
    if (factory_seen_ && !status_seen_) return "factory_first_status";
    if (now - activity_ < 80) return "bus_active";
    return "ready";
  }
  bool ready(uint32_t now) const {
    return !own_ && !passthrough_ && !pending_[0] && !pending_[1] && !held_size_ &&
        !rx_[0].wire_size && !rx_[1].wire_size && now - boot_ >= 5000 &&
        now - activity_ >= 80 && (!quarantine_ || now - fault_at_ >= 2000) &&
        (!factory_seen_ || status_seen_);
  }
  void start(uint32_t now) { boot_ = activity_ = now; }
  void feed(bool factory, uint8_t b, uint32_t now) {
    activity_ = now;
    if (!factory) { history_[history_pos_++ % sizeof(history_)] = b; if (history_size_ < sizeof(history_)) ++history_size_; }
    if (factory && (own_ || quarantine_)) {
      if (held_size_ == sizeof(held_)) {
        // Never silently discard factory bytes. Fail open and stop local injection.
        ++counters.overflows;
        passthrough_ = true; passthrough_reason_ = "factory_buffer_overflow";
        sink_.to_main(held_, held_size_);
        held_size_ = 0;
        sink_.to_main(&b, 1);
      } else if (passthrough_) sink_.to_main(&b, 1);
      else held_[held_size_++] = b;
      return;
    }
    Frame &f = rx_[factory ? 1 : 0];
    int result = f.put(b, now, !factory);
    if (!result) return;
    if (result < 0) {
      ++counters.invalid;
      ++invalid_side[factory ? 1 : 0];
      // Keep a rejected frame rather than overwriting it with its orphan tail.
      if (!invalid_size || now - invalid_at_ > 200) {
        invalid_source = factory ? 1 : 0;
        invalid_size = f.wire_size < sizeof(invalid_wire) ? f.wire_size : sizeof(invalid_wire);
        memcpy(invalid_wire, f.wire, invalid_size);
        if (!factory) {
          error_context_size = history_size_;
          for (size_t i = 0; i < history_size_; ++i)
            error_context[i] = history_[(history_pos_ - history_size_ + i) % sizeof(history_)];
        }
      } else if (invalid_source == (factory ? 1 : 0)) {
        size_t n = f.wire_size;
        if (n > sizeof(invalid_wire) - invalid_size) n = sizeof(invalid_wire) - invalid_size;
        memcpy(invalid_wire + invalid_size, f.wire, n); invalid_size += n;
      }
      invalid_at_ = now;
      fault_(now);
      if (factory) sink_.to_main(f.wire, f.wire_size);
      else {
        sink_.to_factory(f.wire, f.wire_size);
        if (!factory_seen_ && !own_) sink_.to_local(f.wire, f.wire_size);
      }
    } else {
      if (f.short_preamble || f.extra_preamble) ++counters.recovered_preambles;
      packet_(factory, f, now);
    }
    f.clear();
  }
  void tick(uint32_t now) {
    for (unsigned side = 0; side < 2; ++side) {
      Frame &f = rx_[side];
      if (f.wire_size && now - f.last >= 200) {
        ++counters.partial_timeouts;
        if (side) sink_.to_main(f.wire, f.wire_size);
        else sink_.to_factory(f.wire, f.wire_size);
        f.clear(); fault_(now);
      }
      if (pending_[side] && now - pending_at_[side] >= 2000) {
        ++pending_timeouts[side]; timeout_type[side] = pending_[side];
        pending_[side] = 0;
        ++counters.transaction_timeouts;
        fault_(now);
      }
    }
    if (quarantine_ && !own_ && now - fault_at_ >= 2000) {
      quarantine_ = false; late_reply_ = false;
      release_(now);
    }
    // A silent/disconnected factory module must not prevent direct operation.
    // Never change ownership in the middle of a frame or transaction.
    if (factory_seen_ && !passthrough_ && !own_ && !pending_[0] && !pending_[1] &&
        !held_size_ && !rx_[0].wire_size && !rx_[1].wire_size && now - factory_at_ >= 60000) {
      factory_seen_ = status_seen_ = false;
      ++counters.factory_fallbacks;
    }
  }
  // Called by the existing protocol engine only when ready (or for a direct-mode ACK).
  bool send(const uint8_t *data, size_t len, uint32_t now) {
    Frame f;
    int result = 0;
    for (size_t i = 0; i < len; ++i) {
      result = f.put(data[i], now);
      if (result && i + 1 != len) { ++counters.rejected; return false; }
    }
    if (result != 1) { ++counters.rejected; return false; }
    const uint8_t reply = response_(f.type());
    if (!factory_seen_ && f.type() == 0x05) { sink_.to_main(data, len); return true; }
    // Only understood local requests may enter a shared bus.
    if (!ready(now) || reply == UNKNOWN || reply == 0) { ++counters.rejected; return false; }
    own_ = true; answered_ = false; late_reply_ = false; expected_ = reply; own_crc_ = f.crc();
    last_local_type = f.type(); last_local_flags = f.body[1];
    own_sub_ = f.type() == 1 && f.payload_size() >= 2 && f.payload()[0] == 0x4D &&
        f.payload()[1] == 0xFE ? 0x7D : 0x6D;
    ++counters.local_requests;
    sink_.to_main(data, len);
    activity_ = now;
    return true;
  }
  // The protocol engine has consumed its reply or completed its timeout callback.
  void local_finished(uint32_t now) {
    if (!own_) return;
    own_ = false;
    late_reply_ = !answered_ && !passthrough_;
    if (!answered_) { ++counters.transaction_timeouts; ++local_timeouts; fault_(now); }
    if (!quarantine_) release_(now);
  }

 private:
  static constexpr uint8_t UNKNOWN = 0xFF;
  Sink &sink_;
  Frame rx_[2];
  uint8_t held_[2048]{};
  size_t held_size_{0};
  uint32_t boot_{0}, activity_{0}, fault_at_{0}, pending_at_[2]{};
  uint32_t factory_at_{0};
  uint8_t history_[128]{};
  size_t history_pos_{0}, history_size_{0};
  uint32_t invalid_at_{0};
  uint8_t pending_[2]{}, expected_{0}, own_sub_{0};
  bool own_{false}, answered_{false}, own_crc_{false}, factory_seen_{false};
  const char *passthrough_reason_{"none"};
  bool late_reply_{false};
  bool status_seen_{false}, quarantine_{false}, passthrough_{false};
  static uint8_t response_(uint8_t type) {
    switch (type) {
      case 0x01: return 0x02;
      case 0x61: return 0x62;
      case 0x70: return 0x71;
      case 0x73: return 0x74;
      case 0xFC: return 0xFD;
      case 0xF7: case 0x04: case 0x06: case 0x69: return 0x05;
      case 0xF0: return 0xF1;
      case 0xF2: return 0xF3;
      case 0xF4: return 0xF5;
      case 0x02: case 0x03: case 0x05: case 0x62: case 0x71: case 0x74:
      case 0xFD: case 0xF1: case 0xF3: case 0xF5: return 0;
      default: return UNKNOWN;
    }
  }
  void fault_(uint32_t now) { quarantine_ = true; fault_at_ = now; }
  bool matches_(const Frame &f) const {
    if (f.crc() != own_crc_) return false;
    // Our library emits zero address/reserved bytes. Do not steal addressed traffic.
    for (unsigned i = 2; i < 7; ++i) if (f.body[i]) return false;
    if (f.type() == 0x03) return true;
    if (f.type() != expected_) return false;
    return expected_ != 0x02 || (f.payload_size() >= 2 &&
        f.payload()[0] == own_sub_ && f.payload()[1] == 1);
  }
  void packet_(bool factory, const Frame &f, uint32_t now) {
    unsigned side = factory ? 1 : 0;
    if (factory) { factory_seen_ = true; factory_at_ = now; ++counters.factory_frames; }
    else {
      ++counters.main_frames;
      last_main_type = f.type(); last_main_flags = f.body[1];
      memcpy(last_main_address, f.body + 2, sizeof(last_main_address));
      if (f.type() == 0x62) {
        version_flags = f.body[1];
        memcpy(version_address, f.body + 2, sizeof(version_address));
      }
      // Without a second module there is no ownership ambiguity: preserve the
      // original HaierProtocol acceptance rules, including negotiation responses.
      if (own_ && !passthrough_ && !answered_ && (!factory_seen_ || matches_(f))) {
        answered_ = true; ++counters.local_replies;
        sink_.to_local(f.wire, f.wire_size);
        return;
      }
      // A timed-out own response must not complete a subsequently held factory request.
      if (quarantine_ && late_reply_ && !passthrough_ && matches_(f)) return;
    }
    if (factory) sink_.to_main(f.wire, f.wire_size);
    else {
      sink_.to_factory(f.wire, f.wire_size);
      if (!factory_seen_ && !own_) sink_.to_local(f.wire, f.wire_size);
      else if (factory_seen_) sink_.observe(f);
      if (f.type() == 2 && f.payload_size() >= 34 &&
          (f.payload()[0] == 0x6D || f.payload()[0] == 0x7D) && f.payload()[1] == 1)
        status_seen_ = true;
    }
    if (!factory_seen_) return;
    if (pending_[1 - side] && (f.type() == pending_[1 - side] || f.type() == 3)) {
      pending_[1 - side] = 0;
      return;
    }
    uint8_t reply = response_(f.type());
    if (reply) { pending_[side] = reply; pending_at_[side] = now; }
    // Firmware upgrades / baud negotiation cannot safely coexist with local polling.
    if (f.type() >= 0xE1 && f.type() <= 0xEF) {
      passthrough_ = true; passthrough_reason_ = "factory_upgrade_or_baud_change";
    }
  }
  void release_(uint32_t now) {
    size_t n = held_size_;
    uint8_t saved[sizeof(held_)];
    memcpy(saved, held_, n);
    held_size_ = 0;
    for (size_t i = 0; i < n; ++i) feed(true, saved[i], now);
  }
};
}  // namespace haier_inline
