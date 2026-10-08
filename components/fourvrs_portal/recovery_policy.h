// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace haier_management {
// One reset per outage, rearmed only by a minute of fresh status.
// The consumed flag is persisted by the caller before scheduling a reset.
class RecoveryPolicy {
 public:
  static constexpr uint32_t STALE_MS = 180000, HEALTHY_MS = 60000;
  void start(uint32_t now, bool consumed) { boot_ = now; consumed_ = consumed; }
  bool update(uint32_t now, bool seen, uint32_t last, bool deferred, bool injection_blocked = false) {
    if (injection_blocked && !blocked_) blocked_at_ = now;
    blocked_ = injection_blocked;
    const uint32_t age = now - (blocked_ ? blocked_at_ : seen ? last : boot_);
    if (seen && !blocked_ && age < 30000) {
      if (!healthy_) { healthy_ = true; healthy_at_ = now; }
    } else healthy_ = false;
    return !consumed_ && age >= STALE_MS && !deferred;
  }
  bool can_rearm(uint32_t now) const { return consumed_ && healthy_ && now - healthy_at_ >= HEALTHY_MS; }
  void consume() { consumed_ = true; }
  void rearm() { consumed_ = false; }
  bool consumed() const { return consumed_; }
 private:
  uint32_t boot_{0}, healthy_at_{0}, blocked_at_{0};
  bool consumed_{false}, healthy_{false}, blocked_{false};
};
}
