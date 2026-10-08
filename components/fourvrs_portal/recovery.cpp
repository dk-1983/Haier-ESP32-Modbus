// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
#include "version.h"
#include <ctime>
#include <memory>

namespace esphome::fourvrs_portal {
void Portal::recovery_setup_() {
  recovery_storage_ok_ = recovery_store_.begin("haier-recovery", false);
  recovery_policy_.start(millis(), !recovery_storage_ok_ || recovery_store_.getBool("consumed", false));
  recovery_state_ = recovery_storage_ok_ ? "monitoring" : "storage_unavailable";
}

String Portal::recovery_report_() {
  if (!recovery_storage_ok_) return "{\"error\":\"storage_unavailable\"}";
  size_t n = recovery_store_.getBytesLength("report");
  if (!n) return "{\"incident\":null}";
  if (n > 8192) return "{\"error\":\"invalid_report_length\"}";
  std::unique_ptr<char[]> data(new (std::nothrow) char[n + 1]);
  if (!data || recovery_store_.getBytes("report", data.get(), n) != n)
    return "{\"error\":\"report_read_failed\"}";
  data[n] = 0;
  return String(data.get());
}

void Portal::recovery_loop_() {
  const uint32_t now = millis();
  const auto *bridge = climate_->inline_bridge();
  const uint32_t age = seen_status_ ? uint32_t(now - last_status_) : now;
  if (!recovery_sample_count_ || uint32_t(now - recovery_sample_at_) >= 15000) {
    auto &sample = recovery_samples_[recovery_sample_next_];
    sample = {now, age, 0, 0, 0, 0, 0};
    if (bridge) {
      const auto &c = bridge->counters;
      sample.main = c.main_frames; sample.factory = c.factory_frames;
      sample.invalid = c.invalid; sample.timeouts = c.transaction_timeouts;
      sample.overflows = c.overflows;
    }
    recovery_sample_next_ = (recovery_sample_next_ + 1) % 12;
    if (recovery_sample_count_ < 12) ++recovery_sample_count_;
    recovery_sample_at_ = now;
  }
  const bool factory_update = bridge && !strcmp(bridge->passthrough_reason(), "factory_upgrade_or_baud_change");
  const bool overflow_block = bridge && !strcmp(bridge->passthrough_reason(), "factory_buffer_overflow");
  const bool maintenance = updates_busy_() || pending_ || scanning_ || wifi_reset_pending_;
  const bool reset_needed = recovery_policy_.update(now, seen_status_, last_status_, factory_update || maintenance, overflow_block);
  if (recovery_policy_.can_rearm(now) && recovery_storage_ok_) {
    if (recovery_store_.putBool("consumed", false) == 1) recovery_policy_.rearm();
  }
  recovery_state_ = !recovery_storage_ok_ ? "storage_unavailable" : factory_update ? "factory_update_deferred" :
      maintenance ? "maintenance_deferred" : recovery_policy_.consumed() ? "reset_used_waiting_for_healthy_link" : "monitoring";
  if (!reset_needed || !recovery_storage_ok_) return;
  if (recovery_retry_ && uint32_t(now - recovery_retry_at_) < 60000) { recovery_state_ = "report_save_failed"; return; }

  // Persist the evidence BEFORE consuming the reset allowance or rebooting.
  // No credentials, MQTT passwords or Wi-Fi keys are included.
  const char *event = overflow_block ? "bridge_injection_blocked" : "ac_status_timeout";
  String report = String("{\"event\":") + json_string_(event) + ",\"version\":" + json_string_(HAIER_FIRMWARE_VERSION) +
      ",\"action\":\"controller_restart_requested\",\"epoch\":" + String((unsigned long)time(nullptr)) +
      ",\"uptime_ms\":" + String(now) + ",\"status_age_ms\":" + String(age) +
      ",\"mqtt_connected\":" + (mqtt_runtime_.connected ? "true" : "false") +
      ",\"wifi_connected\":" + (WiFi.status() == WL_CONNECTED ? "true" : "false") +
      ",\"free_heap\":" + String(ESP.getFreeHeap()) + ",\"climate\":" + climate_status_(true) + ",\"history\":[";
  for (unsigned i = 0; i < recovery_sample_count_; ++i) {
    const auto &s = recovery_samples_[(recovery_sample_next_ + 12 - recovery_sample_count_ + i) % 12];
    if (i) report += ',';
    report += String("{\"uptime_ms\":") + s.uptime + ",\"age_ms\":" + s.age + ",\"main\":" + s.main +
        ",\"factory\":" + s.factory + ",\"invalid\":" + s.invalid + ",\"timeouts\":" + s.timeouts +
        ",\"overflows\":" + s.overflows + "}";
  }
  report += "]}";
  if (report.length() > 8192 || recovery_store_.putBytes("report", report.c_str(), report.length()) != report.length() ||
      recovery_report_() != report || recovery_store_.putBool("consumed", true) != 1) {
    recovery_state_ = "report_save_failed"; recovery_retry_ = true; recovery_retry_at_ = now;
    ESP_LOGE("recovery", "Diagnostic persistence failed; restart deferred");
    return;
  }
  recovery_policy_.consume();
  ESP_LOGE("recovery", "%s (status age %lu ms). Diagnostic report persisted; restarting ESP32 in 5 seconds", event, (unsigned long)age);
  // Best effort notification; delivery is not claimed. The full report survives offline in NVS.
  mqtt_publish_("diagnostics/recovery", String("{\"event\":") + json_string_(event) + ",\"status_age_ms\":" + age +
      ",\"action\":\"controller_restart\",\"report\":\"/diagnostics/recovery\"}");
  recovery_state_ = "restart_scheduled_report_saved";
  restart_delay_ = 5000; restart_at_ = now; restart_pending_ = true;
}
}
