#include "fourvrs_portal.h"
#include "HonSelfTest.h"
#include "ControlPage.h"
#include "version.h"
#include <cmath>

namespace esphome { namespace fourvrs_portal {
static const char *const TAG = "fourvrs_portal";
static constexpr uint32_t RETRY_MS = 30000, FALLBACK_MS = 60000;
static const char HOME_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Haier Modbus</title><style>body{font:18px system-ui;max-width:640px;margin:32px auto;padding:16px;background:#101827;color:#eff6ff}pre{white-space:pre-wrap}a{color:#6ac8ff}</style><h1>Haier · UART + Modbus</h1><p>Локальное управление кондиционером. <a href="/control">Открыть пульт</a> · <a href="/modbus">Modbus</a> · <a href="/mqtt">MQTT</a></p><p id="connection">Проверка связи…</p><pre id="state"></pre><p><a href="/health">Состояние ESP32-S3</a></p><script>async function refresh(){try{let r=await fetch('/haier/status',{cache:'no-store'});if(!r.ok)throw Error();let s=await r.json();document.getElementById('connection').textContent=s.available?'Получено свежее состояние Haier':'Нет свежего состояния Haier';document.getElementById('state').textContent=JSON.stringify(s,null,2)}catch(e){document.getElementById('connection').textContent='Нет связи с ESP32-S3';document.getElementById('state').textContent=''}}refresh();setInterval(refresh,2000);</script></html>)HTML";

String Portal::json_string_(const String &value) {
  String result = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    uint8_t c = value[i];
    if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
    else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); result += escaped; }
    else result += char(c);
  }
  return result + "\"";
}
String Portal::number_(float n) { return std::isfinite(n) ? String(n, 1) : String("null"); }
bool Portal::valid_(const NetworkConfig &c) {
  size_t s = strnlen(c.ssid, sizeof(c.ssid)), p = strnlen(c.password, sizeof(c.password));
  return s > 0 && s <= 32 && (p == 0 || (p >= 8 && p <= 63));
}
bool Portal::save_(ESPPreferenceObject &pref, const NetworkConfig &config) {
  bool ok = pref.save(&config) && global_preferences->sync();
  if (!ok) ESP_LOGE(TAG, "Network configuration write failed");
  return ok;
}
bool Portal::portal_request_() {
  if (radio_ap_() && web_.client().localIP() == WiFi.softAPIP()) return true;
  web_.send(403, "text/plain", "Connect to the device setup Wi-Fi first."); return false;
}
void Portal::start_portal_() {
  if (radio_ap_()) return;
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.softAP((hostname_ + "-setup").c_str(), setup_password_.c_str()))
    ESP_LOGI(TAG, "Setup AP: %s-setup, http://192.168.4.1", hostname_.c_str());
  else { ESP_LOGE(TAG, "Setup AP failed"); WiFi.enableAP(false); }
}
void Portal::show_setup_() {
  if (!portal_request_()) return;
  web_.sendHeader("Cache-Control", "no-store");
  String page = FPSTR(SETUP_PAGE); page.replace("__TOKEN__", token_);
  web_.send(200, "text/html; charset=utf-8", page);
}
void Portal::finish_scan_() {
  if (!scanning_) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  scan_result_ = "[";
  for (int i = 0; i < n && i < 30; ++i) {
    if (i) scan_result_ += ',';
    scan_result_ += "{\"ssid\":" + json_string_(WiFi.SSID(i)) + ",\"rssi\":" + String(WiFi.RSSI(i)) +
        ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  scan_result_ += ']'; WiFi.scanDelete(); scanning_ = false;
}
void Portal::status_received(const char *data, size_t size) {
  // Accepted payload, with status_message_header_size fixed to zero in this project.
  raw_control_valid_ = size >= 2 + sizeof(raw_control_);
  if (raw_control_valid_) memcpy(&raw_control_, data + 2, sizeof(raw_control_));
  raw_sensors_valid_ = size >= 12 + 22;
  if(raw_sensors_valid_) { memset(&raw_sensors_,0,sizeof(raw_sensors_)); memcpy(&raw_sensors_,data+12,22); }
  seen_status_ = true; last_status_ = millis(); ++status_count_;
  last_payload_ = format_hex_pretty(reinterpret_cast<const uint8_t *>(data), size).c_str();
  if (test_pending_ && status_count_ > test_frame_) {
    if (command_matches_()) {
      hon_()->set_control_method(haier::HonControlMethod::MONITOR_ONLY);
      control_window_ = false;
      if (++test_matches_ >= 2) { test_pending_ = false; test_state_ = "confirmed"; }
    } else { test_matches_ = 0; }
  }
  ESP_LOGI(TAG, "Accepted hOn status #%lu, bytes=%u", (unsigned long)status_count_, unsigned(size));
  // Keep original payload for comparison with the unmodified hOn decoder.
  ESP_LOGD(TAG, "hOn status: %s", format_hex_pretty(reinterpret_cast<const uint8_t *>(data), size).c_str());
}
String Portal::climate_status_() {
  bool fresh = seen_status_ && climate_->valid_connection() && uint32_t(millis() - last_status_) < 30000;
  String out = String("{\"available\":") + (fresh ? "true" : "false") +
      ",\"status_frames\":" + String(status_count_) + ",\"age_ms\":" +
      (seen_status_ ? String(uint32_t(millis() - last_status_)) : String("null"));
  out += ",\"current_temperature\":" + (fresh ? number_(climate_->current_temperature) : String("null"));
  out += ",\"target_temperature\":" + (fresh ? number_(climate_->target_temperature) : String("null"));
  out += ",\"mode\":" + (fresh ? json_string_(String(reinterpret_cast<const __FlashStringHelper *>(climate::climate_mode_to_string(climate_->mode)))) : String("null"));
  out += ",\"fan\":" + (fresh && climate_->fan_mode.has_value() ? json_string_(String(reinterpret_cast<const __FlashStringHelper *>(climate::climate_fan_mode_to_string(*climate_->fan_mode)))) : String("null"));
  out += ",\"power\":" + (fresh ? String(climate_->mode != climate::CLIMATE_MODE_OFF ? "true" : "false") : String("null"));
  out += ",\"control_enabled\":" + String(control_window_ ? "true" : "false");
  out += ",\"command_state\":" + json_string_(test_state_) + ",\"command_matches\":" + String(test_matches_);
  out += ",\"request_id\":" + json_string_(request_id_);
  out += ",\"swing\":" + (fresh ? json_string_(swing_()) : String("null"));
  out += ",\"preset\":" + (fresh ? json_string_(preset_()) : String("null"));
  bool extras = fresh && raw_control_valid_;
  out += ",\"quiet\":" + (extras ? String(raw_control_.quiet_mode ? "true" : "false") : String("null"));
  out += ",\"display\":" + (extras ? String(raw_control_.display_status ? "true" : "false") : String("null"));
  out += ",\"vertical_position\":" + (extras ? json_string_(position_name_(raw_control_.vertical_swing_mode,true)) : String("null"));
  out += ",\"horizontal_position\":" + (extras ? json_string_(position_name_(raw_control_.horizontal_swing_mode,false)) : String("null"));
  out += ",\"last_status_hex\":" + json_string_(last_payload_);
  return out + "}";
}
String Portal::health_() {
  return String("{\"version\":\"" HAIER_FIRMWARE_VERSION "\",\"hostname\":") + json_string_(hostname_) +
      ",\"mac\":" + json_string_(WiFi.macAddress()) + ",\"ip\":" + json_string_(WiFi.localIP().toString()) +
      ",\"wifi\":" + (WiFi.status() == WL_CONNECTED ? "true" : "false") +
      ",\"ap\":" + (radio_ap_() ? "true" : "false") + ",\"ota\":" + (ota_active_ ? "true" : "false") +
      ",\"storage_ok\":" + (storage_ok_ ? "true" : "false") + ",\"free_heap\":" + String(ESP.getFreeHeap()) +
      ",\"wifi_outages\":" + String(wifi_outages_) + ",\"reconnect_attempts\":" + String(reconnect_attempts_) +
      ",\"disconnect_reason\":" + String(disconnect_reason_) +
      ",\"uptime_s\":" + String(millis() / 1000) + ",\"sketch_md5\":" + json_string_(ESP.getSketchMD5()) + "}";
}
bool Portal::test_auth_() {
  if (web_.authenticate("admin", ota_password_.c_str())) return true;
  web_.requestAuthentication(); return false;
}
void Portal::configure_web_() {
  modbus_web_(); mqtt_web_();
  web_.on("/haier/extended", HTTP_POST, [this]() { extended_command_(); });
  web_.on("/diagnostics/wifi-drop", HTTP_POST, [this]() {
    if (!test_auth_()) return;
    if (web_.arg("token") != token_) { web_.send(403,"text/plain","Invalid token"); return; }
    if (wifi_drop_used_ || test_pending_ || pending_ || scanning_ || WiFi.status()!=WL_CONNECTED) {
      web_.send(409,"text/plain","Not ready or test already used this boot"); return;
    }
    wifi_drop_used_=true; wifi_drop_pending_=true; wifi_drop_at_=millis();
    web_.send(202,"application/json","{\"accepted\":true,\"recovery\":\"normal reconnect loop\"}");
  });
  web_.on("/control", HTTP_GET, [this]() {
    if (!test_auth_()) return;
    String page = FPSTR(CONTROL_PAGE); page.replace("__TOKEN__", token_);
    web_.sendHeader("Cache-Control", "no-store");
    web_.send(200, "text/html; charset=utf-8", page);
  });
  web_.on("/haier/test-token", HTTP_GET, [this]() {
    if (!test_auth_()) return;
    web_.sendHeader("Cache-Control", "no-store");
    web_.send(200, "application/json", String("{\"token\":") + json_string_(token_) + "}");
  });
  web_.on("/haier/control", HTTP_POST, [this]() { test_command_(); });
  web_.on("/", HTTP_GET, [this]() {
    if (radio_ap_() && web_.client().localIP() == WiFi.softAPIP()) { show_setup_(); return; }
    web_.send_P(200, "text/html; charset=utf-8", HOME_PAGE);
  });
  web_.on("/wifi", HTTP_GET, [this]() { show_setup_(); });
  web_.on("/network", HTTP_GET, [this]() {
    if (!portal_request_()) return;
    web_.sendHeader("Cache-Control", "no-store");
    web_.send(200, "application/json", String("{\"ssid\":") + json_string_(active_.ssid) +
        ",\"connected\":" + (WiFi.status() == WL_CONNECTED ? "true" : "false") +
        ",\"ip\":" + json_string_(WiFi.localIP().toString()) + ",\"reason\":" + String(disconnect_reason_) + "}");
  });
  web_.on("/scan", HTTP_POST, [this]() {
    if (!portal_request_()) return;
    if (web_.arg("token") != token_) { web_.send(403, "text/plain", "Reload setup page."); return; }
    if (pending_) { web_.send(409, "text/plain", "Connection pending."); return; }
    if (!scanning_) {
      WiFi.scanDelete(); scanning_ = true;
      if (WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) {
        scanning_ = false; web_.send(503, "text/plain", "Scan failed; retry."); return;
      }
    }
    web_.send(202, "application/json", "{}");
  });
  web_.on("/scan", HTTP_GET, [this]() {
    if (!portal_request_()) return;
    web_.sendHeader("Cache-Control", "no-store");
    web_.send(scanning_ ? 202 : 200, "application/json", scanning_ ? "{}" : scan_result_);
  });
  web_.on("/wifi", HTTP_POST, [this]() {
    if (!portal_request_()) return;
    if (web_.arg("token") != token_) { web_.send(403, "text/plain", "Reload setup page."); return; }
    if (pending_ || scanning_) { web_.send(409, "text/plain", "Wait for scan/connection."); return; }
    String s = web_.arg("ssid"), p = web_.arg("password");
    if (s.length() == 0 || s.length() > 32 || p.length() > 63 || (p.length() && p.length() < 8) ||
        strlen(s.c_str()) != s.length() || strlen(p.c_str()) != p.length()) {
      web_.send(400, "text/plain", "SSID: 1..32 bytes; password: empty or 8..63 bytes."); return;
    }
    NetworkConfig next{}; s.toCharArray(next.ssid, sizeof(next.ssid)); p.toCharArray(next.password, sizeof(next.password));
    if (!save_(candidate_pref_, next)) { web_.send(503, "text/plain", "Could not save network."); return; }
    active_ = next; pending_ = true; candidate_ = true; pending_at_ = millis();
    web_.send(200, "text/plain; charset=utf-8", "Настройки сохранены. Подключаемся…");
  });
  web_.on("/health", HTTP_GET, [this]() { web_.sendHeader("Cache-Control", "no-store"); web_.send(200, "application/json", health_()); });
  web_.on("/haier/status", HTTP_GET, [this]() { web_.sendHeader("Cache-Control", "no-store"); web_.send(200, "application/json", climate_status_()); });
  web_.onNotFound([this]() { web_.send(404, "text/plain", "Not found"); });
  web_.begin();
}
void Portal::configure_ota_() {
  ArduinoOTA.setHostname(hostname_.c_str()); ArduinoOTA.setPort(8266);
  ArduinoOTA.setPassword(ota_password_.c_str());
  ArduinoOTA.onStart([]() { ESP_LOGI(TAG, "OTA started"); });
  ArduinoOTA.onEnd([]() { ESP_LOGI(TAG, "OTA complete"); });
  ArduinoOTA.onError([](ota_error_t e) { ESP_LOGW(TAG, "OTA error %u", unsigned(e)); });
}
void Portal::setup() {
  bool test_ok = hon_self_test();
  ESP_LOGI(TAG, "Isolated hOn regression tests: %s", test_ok ? "PASS" : "FAIL");
  if (!test_ok) { mark_failed(); return; }
  saved_pref_ = global_preferences->make_preference<NetworkConfig>(0x48504301, true);
  candidate_pref_ = global_preferences->make_preference<NetworkConfig>(0x48504302, true);
  if (!saved_pref_.load(&saved_) || !valid_(saved_)) saved_ = NetworkConfig{};
  NetworkConfig next{};
  candidate_ = candidate_pref_.load(&next) && valid_(next);
  active_ = candidate_ ? next : saved_;
  char suffix[7]; snprintf(suffix, sizeof(suffix), "%06x", uint32_t(ESP.getEfuseMac() >> 24) & 0xffffff);
  hostname_ = String("haier-s3-") + suffix;
  char token[33];
  snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  token_ = token;
  WiFi.persistent(false); WiFi.setHostname(hostname_.c_str()); WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); WiFi.setSleep(false);
  WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) { if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) disconnect_reason_ = info.wifi_sta_disconnected.reason; });
  if (!active_.ssid[0]) start_portal_(); else WiFi.begin(active_.ssid, active_.password);
  modbus_setup_(); mqtt_setup_(); configure_ota_(); configure_web_(); outage_since_ = last_attempt_ = millis();
  ESP_LOGI(TAG, "4VRS portal adapted for ESP32-S3; %s; setup complete", hostname_.c_str());
}
void Portal::loop() {
  mqtt_loop_(); web_.handleClient(); finish_scan_(); modbus_loop_();
  // HTTP handlers can start timers; sample time AFTER handling the request.
  uint32_t now = millis();
  if (wifi_drop_pending_ && uint32_t(now-wifi_drop_at_)>=500) {
    wifi_drop_pending_=false;
    // Fault injection only: normal production reconnect logic below recovers.
    WiFi.disconnect(false, false);
  }
  if (test_pending_ && uint32_t(now - test_started_) >= 30000) {
    hon_()->set_control_method(haier::HonControlMethod::MONITOR_ONLY);
      control_window_ = false;
    hon_()->clear_bridge_overrides();
    climate_->reset_protocol();
    test_pending_ = false; test_state_ = "timeout_unconfirmed";
  }
  if (pending_ && uint32_t(now - pending_at_) >= 300) {
    pending_ = false; WiFi.disconnect(false, false); connected_before_ = false;
    if (ota_active_) { ArduinoOTA.end(); ota_active_ = false; }
    outage_since_ = last_attempt_ = now; WiFi.begin(active_.ssid, active_.password);
  }
  if (WiFi.status() == WL_CONNECTED && !pending_) {
    if (!connected_before_ || last_ip_ != WiFi.localIP()) {
      connected_before_ = true; connected_since_ = now; last_ip_ = WiFi.localIP();
      storage_ok_ = true;
      if (memcmp(&saved_, &active_, sizeof(saved_)) != 0) {
        storage_ok_ = save_(saved_pref_, active_); if (storage_ok_) saved_ = active_;
      }
      if (candidate_ && storage_ok_) { NetworkConfig empty{}; storage_ok_ = save_(candidate_pref_, empty); if (storage_ok_) candidate_ = false; }
      ESP_LOGI(TAG, "Wi-Fi IP: %s; saved=%s", last_ip_.toString().c_str(), storage_ok_ ? "yes" : "NO");
      if (ota_active_) { ArduinoOTA.end(); }
      ArduinoOTA.begin(); ota_active_ = true;
    }
    if (radio_ap_() && storage_ok_ && !scanning_ && uint32_t(now - connected_since_) >= 15000 && uint32_t(now - last_stop_) >= 1000) {
      last_stop_ = now; WiFi.enableAP(false); ESP_LOGI(TAG, "Setup AP active=%s", radio_ap_() ? "yes" : "no");
    }
    ArduinoOTA.handle();
  } else {
    if (connected_before_) {
      ++wifi_outages_;
      connected_before_ = false; outage_since_ = last_attempt_ = now;
      if (ota_active_) { ArduinoOTA.end(); ota_active_ = false; }
    }
    if (candidate_ && saved_.ssid[0] && uint32_t(now - outage_since_) >= FALLBACK_MS) {
      NetworkConfig empty{};
      if (save_(candidate_pref_, empty)) {
        candidate_ = false; active_ = saved_; WiFi.disconnect(false, false); WiFi.begin(active_.ssid, active_.password);
        last_attempt_ = now; ESP_LOGW(TAG, "Candidate failed; restoring previous network");
      }
      start_portal_();
    }
    if (!active_.ssid[0] || uint32_t(now - outage_since_) >= FALLBACK_MS) start_portal_();
    if (!scanning_ && active_.ssid[0] && uint32_t(now - last_attempt_) >= RETRY_MS) {
      last_attempt_ = now; ++reconnect_attempts_; WiFi.begin(active_.ssid, active_.password);
    }
  }
}
}}  // namespace esphome::fourvrs_portal
