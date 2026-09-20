// SPDX-License-Identifier: MIT
#pragma once
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/log.h"
#include <cstring>
static uint8_t loopback_expected[32];
static unsigned loopback_round=0;
inline void haier_loopback_begin(esphome::uart::UARTComponent *uart) {
  uint8_t discard; unsigned drained=0;
  while(uart->available() && drained<512) { if(!uart->read_byte(&discard)) break; ++drained; }
  ++loopback_round;
  const uint8_t pattern[]={0x00,0xff,0x55,0xaa,0x01,0x80,0x7f,0xfe};
  for(unsigned i=0;i<32;++i)loopback_expected[i]=pattern[i%8] ^ uint8_t(loopback_round+i/8);
  uart->write_array(loopback_expected,32);
  ESP_LOGI("loopback", "Round %u: TX 32 bytes on GPIO17; expecting return on GPIO18",loopback_round);
}
inline void haier_loopback_finish(esphome::uart::UARTComponent *uart) {
  uint8_t actual[64]; unsigned n=0;
  while(uart->available() && n<sizeof(actual)) { if(!uart->read_byte(&actual[n])) break; ++n; }
  bool ok=n==32 && !uart->available() && !std::memcmp(actual,loopback_expected,32);
  if(ok) ESP_LOGI("loopback", "PASS round %u: all 32 bytes match (9600 8N1, no inversion/pulls)",loopback_round);
  else {
    ESP_LOGW("loopback", "FAIL round %u: RX %u/32 bytes; buffered=%u",loopback_round,n,unsigned(uart->available()));
    for(unsigned i=0;i<n && i<32;++i)if(actual[i]!=loopback_expected[i]) {ESP_LOGW("loopback", "First mismatch [%u]: expected %02X got %02X",i,loopback_expected[i],actual[i]);break;}
  }
}
