// SPDX-License-Identifier: MIT
#pragma once
#include "driver/gpio.h"
#include "esphome/core/log.h"
// Bench only: no Haier or other external signal connection on GPIO17/18.
inline void haier_gpio_test_step(unsigned step) {
  gpio_config_t cfg{};
  cfg.pin_bit_mask = (1ULL << 17) | (1ULL << 18);
  cfg.mode = GPIO_MODE_INPUT;
  cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  cfg.intr_type = GPIO_INTR_DISABLE;
  auto err = gpio_config(&cfg);
  if (err != ESP_OK) { ESP_LOGE("gpio_test", "Input setup error %d", int(err)); return; }
  gpio_num_t pin = step < 2 ? GPIO_NUM_17 : GPIO_NUM_18;
  int level = step & 1;
  err = gpio_set_level(pin, level);
  if (err == ESP_OK) err = gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  if (err != ESP_OK) { ESP_LOGE("gpio_test", "Output setup error %d", int(err)); return; }
  ESP_LOGI("gpio_test", "STEP %u/4 GPIO%d = %s for 5s; other pin INPUT; no pulls", step + 1, int(pin), level ? "HIGH" : "LOW");
}
