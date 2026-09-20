// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <mqtt_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "mqtt_model.h"
namespace esphome::fourvrs_portal {
struct MqttConfig {
  uint32_t magic{0x484d5101};uint16_t port{1883};uint8_t enabled{0},discovery_disabled{0};
  char host[128]{},username[65]{},password[129]{},prefix[65]{};
};
struct MqttPublish {char topic[128]{};char payload[1536]{};uint32_t epoch{0};bool state{false};};
// Worker owns the MQTT handle and all blocking library calls; loop() only uses queues.
struct MqttRuntime {
  QueueHandle_t config_queue{nullptr},rx_queue{nullptr},tx_queue{nullptr};
  TaskHandle_t task{nullptr};esp_mqtt_client_handle_t client{nullptr};
  MqttConfig active{};haier_bridge::MqttAssembly assembly{};
  std::atomic<bool> connected{false},applying{true},state_pending{false};
  std::atomic<uint32_t> state_epoch{0},discovery_request{0},discovery_acked{0};std::atomic<uint32_t> connections{0},dropped{0};
  std::atomic<int> last_error{0},last_ack{-1};
  char client_id[40]{},availability[128]{},subscription[128]{};
};
}
