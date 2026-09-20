// SPDX-License-Identifier: MIT
// Bench only. No connection to the real conditioner.
#include <SoftwareSerial.h>
#include "hon_simulator.h"

// Nano/Uno ATmega328P: keep USB hardware Serial exclusively for diagnostics.
SoftwareSerial haier(10, 11); // RX D10, TX D11, non-inverted 9600 8N1
hon_bench::Parser parser;
hon_bench::Appliance appliance;
hon_bench::Message request, response;
uint32_t sent = 0, overflowCount = 0, lastReport = 0, ledUntil = 0;
bool muteReplies = false, corruptNext = false;

// Optional checksum fault, exercised only by explicit USB command 'c'.
struct WireOutput {
  bool corrupt = false;
  uint8_t position = 0;
  void write(uint8_t b) {
    // Toggle an unused header byte, preserving frame size and escaping.
    if (corrupt && position == 4) b ^= 1;
    ++position; haier.write(b);
  }
};

void report() {
  Serial.print(F("bytes=")); Serial.print(parser.bytes);
  Serial.print(F(" good=")); Serial.print(parser.good);
  Serial.print(F(" bad=")); Serial.print(parser.bad);
  Serial.print(F(" partial_timeout=")); Serial.print(parser.timeouts);
  Serial.print(F(" overflow=")); Serial.print(overflowCount);
  Serial.print(F(" sent=")); Serial.print(sent);
  Serial.print(F(" commands=")); Serial.print(appliance.commands);
  Serial.print(F(" unsupported=")); Serial.print(appliance.unsupported);
  Serial.print(F(" mute=")); Serial.print(muteReplies);
  Serial.print(F(" power=")); Serial.print(appliance.control[5] & 1);
  Serial.print(F(" target=")); Serial.print(16 + appliance.control[0]);
  Serial.print(F(" mode=")); Serial.print(appliance.control[2] >> 5);
  Serial.print(F(" fan=")); Serial.print(appliance.control[2] & 7);
  Serial.print(F(" flags=0x")); Serial.println(appliance.control[5], HEX);
}
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200); haier.begin(9600);
  Serial.println(F("Haier bench simulator 0.1.0; NOT a real AC"));
  Serial.println(F("UART RX=D10 TX=D11 9600 8N1; USB=115200"));
  Serial.println(F("USB: s=status, m=mute replies, r=resume, c=corrupt next reply"));
}
void loop() {
  parser.expire(millis());
  if (haier.overflow()) ++overflowCount;
  while (haier.available()) {
    if (!parser.feed(uint8_t(haier.read()), millis(), request)) continue;
    digitalWrite(LED_BUILTIN, HIGH); ledUntil = millis() + 80;
    if (appliance.reply(request, response) && !muteReplies) {
      delay(15); // bench appliance turnaround, well below ESP answer timeout
      WireOutput wire; wire.corrupt = corruptNext; corruptNext = false;
      hon_bench::send(wire, response); ++sent;
    }
    Serial.print(F("RX type=0x")); Serial.print(request.type, HEX);
    Serial.print(F(" size=")); Serial.print(request.size);
    Serial.print(F(" crc=")); Serial.println(request.crc);
  }
  if (ledUntil && int32_t(millis() - ledUntil) >= 0) {
    digitalWrite(LED_BUILTIN, LOW); ledUntil = 0;
  }
  while (Serial.available()) {
    switch (Serial.read()) {
      case 's': report(); break;
      case 'm': muteReplies = true; Serial.println(F("Replies muted")); break;
      case 'r': muteReplies = false; Serial.println(F("Replies resumed")); break;
      case 'c': corruptNext = true; Serial.println(F("Next reply corrupted once")); break;
    }
  }
  if (uint32_t(millis() - lastReport) >= 5000 && !haier.available()) {
    lastReport = millis(); report();
  }
}
