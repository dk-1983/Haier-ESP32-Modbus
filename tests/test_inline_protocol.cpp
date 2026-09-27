// End-to-end transport check against the actual HaierProtocol dependency.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <deque>
#include <thread>
#include <vector>
#include "protocol/haier_protocol.h"
#include "components/haier/inline_bridge.h"
#include "bench/haier_simulator/hon_simulator.h"

struct Output {
  std::vector<uint8_t> bytes;
  void write(uint8_t b) { bytes.push_back(b); }
};
struct Harness : haier_inline::Sink, haier_protocol::ProtocolStream {
  haier_inline::Bridge bridge{*this};
  haier_protocol::ProtocolHandler protocol{*this};
  hon_bench::Parser parser;
  hon_bench::Appliance appliance;
  std::deque<uint8_t> incoming;
  std::vector<uint8_t> request, factory;
  uint32_t now = 6000;
  unsigned replies = 0, timeouts = 0;
  Harness() {
    bridge.start(0);
    protocol.set_cooldown_interval(0);
    protocol.set_answer_timeout(30);
    protocol.set_default_answer_handler([this](auto, auto, const uint8_t *, size_t) {
      ++replies; return haier_protocol::HandlerError::HANDLER_OK;
    });
    protocol.set_default_timeout_handler([this](auto) {
      ++timeouts; return haier_protocol::HandlerError::HANDLER_OK;
    });
  }
  size_t available() noexcept override { return incoming.size(); }
  size_t read_array(uint8_t *p, size_t n) noexcept override {
    if (incoming.size() < n) return 0;
    for (size_t i = 0; i < n; ++i) { p[i] = incoming.front(); incoming.pop_front(); }
    return n;
  }
  void write_array(const uint8_t *p, size_t n) noexcept override { assert(bridge.send(p, n, now)); }
  void to_main(const uint8_t *p, size_t n) override { request.insert(request.end(), p, p+n); }
  void to_factory(const uint8_t *p, size_t n) override { factory.insert(factory.end(), p, p+n); }
  void to_local(const uint8_t *p, size_t n) override { incoming.insert(incoming.end(), p, p+n); }
  void observe(const haier_inline::Frame &) override {}
  void reply(int delimiter_change = 0) {
    hon_bench::Message in, out;
    auto bytes = request; request.clear();
    for (auto v : bytes) if (parser.feed(v, now, in) && appliance.reply(in, out)) {
      Output encoded; hon_bench::send(encoded, out);
      if (delimiter_change < 0) encoded.bytes.erase(encoded.bytes.begin());
      if (delimiter_change > 0) encoded.bytes.insert(encoded.bytes.begin(),0xFF);
      for (auto b : encoded.bytes) bridge.feed(false, b, now);
    }
  }
  void loop() {
    if (bridge.busy() || bridge.ready(now)) {
      protocol.loop();
      if (bridge.busy() && !protocol.is_waiting_for_answer()) bridge.local_finished(now);
    }
  }
};
int main() {
  using namespace haier_protocol;
  Harness h;
  for (auto type : {FrameType::GET_DEVICE_VERSION, FrameType::GET_DEVICE_ID, FrameType::GET_ALARM_STATUS}) {
    h.protocol.send_message(HaierMessage(type), false);
    h.loop(); assert(h.protocol.is_waiting_for_answer()); h.reply(); h.loop();
    assert(!h.bridge.busy() && !h.protocol.is_waiting_for_answer()); h.now += 100;
  }
  assert(h.replies == 3);
  for (unsigned i = 0; i < 20; ++i) {
    h.protocol.send_message(HaierMessage(FrameType::CONTROL, uint16_t(0x4D01)), true);
    h.loop(); h.reply(i % 2 == 0 ? -1 : 1); h.loop(); h.now += 100;
  }
  assert(h.replies == 23 && h.factory.empty() && h.timeouts == 0);
  assert(h.bridge.counters.recovered_preambles == 20 && h.bridge.counters.invalid == 0);
  h.protocol.send_message(HaierMessage(FrameType::CONTROL, uint16_t(0x4D01)), true);
  h.loop();
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  h.now += 40; h.loop();
  assert(h.timeouts == 1 && !h.bridge.busy() && !h.bridge.ready(h.now));
  h.reply(); assert(h.incoming.empty() && h.factory.empty());
  puts("PASS: real HaierProtocol handshake, 20 polls, response isolation and timeout");
}
