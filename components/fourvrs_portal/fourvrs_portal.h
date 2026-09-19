#pragma once
// Adapted from 4vrs-display/rack_bootstrap. See LICENSE-4VRS.
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include "esphome/components/haier/hon_climate.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <esp_random.h>
#include "SetupPage.h"
#include "ModbusPage.h"
#include "modbus_core.h"
#include "mqtt_client_state.h"

namespace esphome { namespace fourvrs_portal {
class Portal : public Component, public haier_bridge::Backend {
 public:
  void set_climate(haier::HaierClimateBase *value) { climate_ = value; }
  void set_setup_password(const std::string &s) { setup_password_ = s.c_str(); }
  void set_ota_password(const std::string &s) { ota_password_ = s.c_str(); }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_rs485(uart::UARTComponent *value) { rs485_ = value; }
  uint8_t read(haier_bridge::Table table, uint16_t address, uint16_t &value) override;
  uint8_t write(const haier_bridge::Change *changes, size_t count) override;
  void setup() override;
  void loop() override;
  void status_received(const char *data, size_t size);
 protected:
  MqttConfig mqtt_config_{};MqttRuntime mqtt_runtime_{};ESPPreferenceObject mqtt_pref_;
  uint32_t mqtt_last_publish_{0},mqtt_seen_connections_{0},mqtt_command_count_{0};
  String mqtt_pending_id_;
  void mqtt_setup_();void mqtt_web_();void mqtt_loop_();String mqtt_json_();
  bool mqtt_publish_(const char *suffix,const String &body);
  struct ModbusConfig { uint32_t magic{0x484d4201}; uint32_t baud{19200}; uint8_t unit{1}, rtu{0}, tcp{0}, reserved{0}; };
  ModbusConfig modbus_config_{};
  ESPPreferenceObject modbus_pref_;
  uart::UARTComponent *rs485_{nullptr};
  WiFiServer modbus_server_{502};
  struct ClientSlot { WiFiClient client; haier_bridge::TcpFrame frame; uint32_t last_ms{0}; };
  ClientSlot modbus_clients_[2];
  bool modbus_listening_{false},modbus_dirty_{false},modbus_pending_{false};
  uint32_t modbus_commands_{0},rtu_last_us_{0};
  uint8_t rtu_bytes_[256]{};size_t rtu_used_{0};
  uint16_t modbus_mask_{0};
  haier::hon_protocol::HaierPacketControl modbus_expected_{};
  haier::hon_protocol::HaierPacketSensors raw_sensors_{};
  bool raw_sensors_valid_{false};
  void modbus_setup_();void modbus_loop_();void modbus_web_();
  String modbus_json_();bool modbus_matches_() const;
  struct NetworkConfig { char ssid[33]{}; char password[64]{}; };
  WebServer web_{80};
  ESPPreferenceObject saved_pref_, candidate_pref_;
  NetworkConfig saved_{}, active_{};

  haier::HaierClimateBase *climate_{nullptr};
  String hostname_, token_, setup_password_, ota_password_, scan_result_{"[]"};
  bool candidate_{false}, pending_{false}, scanning_{false}, connected_before_{false};
  bool ota_active_{false}, storage_ok_{true}, seen_status_{false};
  uint32_t outage_since_{0}, last_attempt_{0}, connected_since_{0}, last_stop_{0};
  uint32_t last_status_{0}, status_count_{0}, pending_at_{0};
  unsigned disconnect_reason_{0};
  bool wifi_drop_pending_{false}, wifi_drop_used_{false};
  uint32_t wifi_drop_at_{0}, reconnect_attempts_{0}, wifi_outages_{0};
  IPAddress last_ip_;
  static String json_string_(const String &s);
  static String number_(float n);
  static bool valid_(const NetworkConfig &c);
  bool save_(ESPPreferenceObject &pref, const NetworkConfig &config);
  bool radio_ap_() const { return (WiFi.getMode() & WIFI_AP) != 0; }
  bool portal_request_();
  void start_portal_();
  void show_setup_();
  void finish_scan_();
  void configure_web_();
  void configure_ota_();
  bool test_pending_{false}, control_window_{false};
  uint32_t test_started_{0}, test_frame_{0};
  unsigned test_matches_{0};
  String test_state_{"not_started"}, last_payload_;
  haier::HonClimate *hon_() { return static_cast<haier::HonClimate *>(climate_); }
  bool test_auth_();
  void test_command_();
  bool command_matches_() const;
  void extended_command_();
  int desired_quiet_{-1}, desired_display_{-1}, desired_vertical_{-1}, desired_horizontal_{-1};
  bool raw_control_valid_{false};
  haier::hon_protocol::HaierPacketControl raw_control_{};
  static int position_value_(const String &name, bool vertical);
  static String position_name_(int value, bool vertical);
  String desired_mode_, desired_fan_, desired_swing_, desired_preset_, request_id_;
  bool desired_target_set_{false};
  float desired_target_{NAN};
  static String log_string_(const LogString *s) { return String(reinterpret_cast<const __FlashStringHelper *>(s)); }
  String preset_() const { return climate_->preset.has_value() ? log_string_(climate::climate_preset_to_string(*climate_->preset)) : String("NONE"); }
  String swing_() const { return log_string_(climate::climate_swing_mode_to_string(climate_->swing_mode)); }
  String health_();
  String climate_status_();
};
}}  // namespace esphome::fourvrs_portal
