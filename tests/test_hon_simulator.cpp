// SPDX-License-Identifier: MIT
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstddef>
#include "haier_simulator/hon_simulator.h"
#include "transport/haier_frame.h"
using namespace hon_bench;
struct Output { std::vector<uint8_t> bytes; void write(uint8_t b) { bytes.push_back(b); } };
uint32_t now = 0;
unsigned feed(Parser &p, const std::vector<uint8_t> &wire, Message &m) {
  unsigned n = 0; for (auto b : wire) if (p.feed(b, ++now, m)) ++n; return n;
}
std::vector<uint8_t> upstream(const Message &m) {
  haier_protocol::HaierFrame f(m.type, m.data, m.size, m.crc);
  std::vector<uint8_t> w(f.get_buffer_size());
  assert(f.fill_buffer(w.data(), w.size()) == w.size()); return w;
}
void upstreamAccepts(const Message &m) {
  Output o; send(o, m);
  // Transport removes FF55 stuffing before calling HaierFrame in two parts.
  std::vector<uint8_t> raw = {0xFF, 0xFF};
  for (size_t i = 2; i < o.bytes.size(); ++i) {
    raw.push_back(o.bytes[i]);
    if (o.bytes[i] == 0xFF) { assert(o.bytes.at(++i) == 0x55); }
  }
  haier_protocol::HaierFrame f; haier_protocol::FrameError e;
  f.parse_buffer(raw.data(), 10, e);
  assert(e == haier_protocol::FrameError::HEADER_ONLY);
  f.parse_buffer(raw.data() + 10, raw.size() - 10, e);
  assert(e == haier_protocol::FrameError::COMPLETE_FRAME);
  assert(f.get_data_size() == m.size);
}
int main() {
  Parser parser; Message got;
  // Independent upstream encoder exercises CRC, checksum and FF55 escaping.
  uint32_t rng = 73;
  for (unsigned j = 0; j < 2000; ++j) {
    Message m; m.type = 1; m.crc = j % 2; m.size = j % 65;
    for (uint8_t i = 0; i < m.size; ++i) {
      rng = rng * 1664525u + 1013904223u;
      m.data[i] = (j % 7 == 0) ? 0xFF : uint8_t(rng >> 24);
    }
    const auto wire = upstream(m);
    Output encoded; send(encoded, m); assert(encoded.bytes == wire);
    assert(feed(parser, wire, got) == 1);
    assert(got.type == m.type && got.size == m.size && got.crc == m.crc);
    assert(memcmp(got.data, m.data, m.size) == 0);
    upstreamAccepts(m);
  }
  Message m; m.type = 0x61; m.size = 2; m.data[1] = 7;
  auto wire = upstream(m);
  auto bad = wire; bad.back() ^= 1;
  assert(feed(parser, bad, got) == 0);
  assert(feed(parser, wire, got) == 1);
  m.crc = true; wire = upstream(m); bad = wire; bad.back() ^= 1;
  assert(feed(parser, bad, got) == 0);
  assert(feed(parser, wire, got) == 1);
  feed(parser, std::vector<uint8_t>(wire.begin(), wire.begin() + 6), got);
  now += 101; parser.expire(now); assert(parser.timeouts == 1);
  assert(feed(parser, wire, got) == 1);
  // New preamble replaces a truncated frame without needing timeout.
  feed(parser, std::vector<uint8_t>(wire.begin(), wire.begin() + 5), got);
  assert(feed(parser, wire, got) == 1);
  assert(feed(parser, {0xFF,0xFF,0xFF,0x55}, got) == 0); // length overflow
  assert(feed(parser, wire, got) == 1);
  auto doubled = wire; doubled.insert(doubled.end(), wire.begin(), wire.end());
  assert(feed(parser, doubled, got) == 2);

  Appliance ac; Message answer;
  assert(ac.reply(m, answer) && answer.type == 0x62 && answer.size == 38);
  assert(answer.data[37] == 4); upstreamAccepts(answer);
  for (auto type : {0x70, 0x73, 0xFC, 0xF7}) {
    m.type = type; m.size = 0; assert(ac.reply(m, answer)); upstreamAccepts(answer);
  }
  m.type = 1; m.size = 2; m.data[0] = 0x4D; m.data[1] = 1;
  assert(ac.reply(m, answer) && answer.size == 34 && answer.data[12] == 47);
  assert(answer.data[2] == 8 && answer.data[7] == 0);
  // Same group write used by production portal, persisted into later polls.
  m.size = 12; m.data[0] = 0x60;
  memcpy(m.data + 2, ac.control, 10);
  m.data[2] = 10; m.data[4] = 0x81; m.data[7] = 0x19;
  assert(ac.reply(m, answer) && answer.data[2] == 10 && answer.data[7] == 0x19);
  assert(ac.commands == 1); upstreamAccepts(answer);
  m.size = 2; m.data[0] = 0x4D;
  assert(ac.reply(m, answer) && answer.data[7] == 0x19);
  m.data[1] = 0xFE; assert(ac.reply(m, answer) && answer.size == 48);
  upstreamAccepts(answer);
  m.size = 4; m.data[0] = 0x5D; m.data[1] = 1; m.data[2] = m.data[3] = 0;
  assert(ac.reply(m, answer) && answer.data[7] == 0x18);
  m.size = 2; m.data[0] = 0x60; m.data[1] = 1;
  assert(ac.reply(m, answer) && answer.type == 3 && ac.commands == 2);
  m.type = 0x05; assert(!ac.reply(m, answer));
  puts("PASS: 2000 upstream wire/CRC cases; corruption, recovery, timeout, handshake, status and controls");
}
