#include "components/haier/inline_bridge.h"
#include "bench/haier_simulator/hon_simulator.h"
#include <cassert>
#include <cstdio>
#include <vector>
using Bytes = std::vector<uint8_t>;
struct Output {
  Bytes bytes;
  void write(uint8_t b) { bytes.push_back(b); }
};
Bytes wire(uint8_t type, bool crc = true, Bytes payload = {}) {
  assert(payload.size() <= hon_bench::MAX_PAYLOAD);
  hon_bench::Message m;
  m.type = type; m.crc = crc; m.size = payload.size();
  std::copy(payload.begin(), payload.end(), m.data);
  Output out; hon_bench::send(out, m); return out.bytes;
}
Bytes status() { Bytes p(34); p[0] = 0x6D; p[1] = 1; return wire(2, true, p); }
struct Sink : haier_inline::Sink {
  Bytes main, factory, local;
  unsigned observations = 0;
  void to_main(const uint8_t *p, size_t n) override { main.insert(main.end(), p, p+n); }
  void to_factory(const uint8_t *p, size_t n) override { factory.insert(factory.end(), p, p+n); }
  void to_local(const uint8_t *p, size_t n) override { local.insert(local.end(), p, p+n); }
  void observe(const haier_inline::Frame &) override { ++observations; }
};
void feed(haier_inline::Bridge &b, bool factory, const Bytes &bytes, uint32_t now) {
  for (auto v : bytes) b.feed(factory, v, now);
}
const Bytes poll = wire(1, true, {0x4D, 1});
void init(haier_inline::Bridge &b, uint32_t start = 0) {
  b.start(start);
  feed(b, true, poll, start + 100);
  feed(b, false, status(), start + 170);
  assert(!b.ready(start + 4999));
  assert(b.ready(start + 5000));
}
int main() {
  { // Regression: exact ACK and status captured in the 30-minute hardware run.
    const char *captures[]={"FFFFFF08400000000000054D6180",
      "FFFFFF2A400000000000026D01070623000201000000002E00480000030000000000000000000000000000000086C8CA"};
    for (const char *hex : captures) {
      Bytes captured;
      for (size_t i=0;hex[i];i+=2) { unsigned v; assert(sscanf(hex+i,"%2x",&v)==1); captured.push_back(uint8_t(v)); }
      Bytes canonical(captured.begin()+1,captured.end());
      Sink s; haier_inline::Bridge b(s); init(b); s.factory.clear();
      feed(b,true,canonical[9]==5 ? wire(0xF7) : poll,6000);
      feed(b,false,captured,6100);b.tick(9000);
      assert(s.factory==canonical && b.ready(9100));
      assert(b.counters.invalid==0 && b.counters.transaction_timeouts==0);
      assert(b.counters.recovered_preambles==1);
      // Corruption must stay byte-for-byte transparent, never become a reply.
      Sink bad;haier_inline::Bridge damaged(bad);damaged.start(0);
      auto corrupt=captured;corrupt.back()^=1;feed(damaged,false,corrupt,100);
      assert(bad.factory==corrupt && damaged.counters.main_frames==0);
      assert(damaged.counters.recovered_preambles==0);
      haier_inline::Frame strict;int result=0;
      for(auto v:captured) { result=strict.put(v,100,false);if(result)break; }
      assert(result==-1 && !strict.extra_preamble);
    }
  }
  { // Bound repair to a single extra FF; require CRC and preserve length stuffing.
    for (unsigned length : {8u, 84u, 255u}) {
      Bytes body(length,0xFF);body[0]=uint8_t(length);body[1]=0x40;
      Output encoded;encoded.bytes={0xFF,0xFF};uint8_t sum=0;uint16_t check=0;
      for(auto v:body) {sum+=v;if(v==0xFF)sum+=0x55;check=hon_bench::crcByte(check,v);hon_bench::escaped(encoded,v);}
      hon_bench::escaped(encoded,sum);hon_bench::escaped(encoded,uint8_t(check>>8));hon_bench::escaped(encoded,uint8_t(check));
      auto canonical=encoded.bytes;
      for (bool extra : {false,true}) {
        auto bytes=canonical;if(extra)bytes.insert(bytes.begin(),0xFF);
        haier_inline::Frame f;
        for(size_t i=0;i<bytes.size();++i)
          assert(f.put(bytes[i],uint32_t(i),true)==(i+1==bytes.size()?1:0));
        assert(Bytes(f.wire,f.wire+f.wire_size)==canonical);
        assert(f.extra_preamble==extra);
      }
    }
    for (bool crc : {false,true}) {
      auto bytes=wire(5,crc);bytes.insert(bytes.begin(),crc?2:1,0xFF);
      Sink s;haier_inline::Bridge b(s);b.start(0);feed(b,false,bytes,100);b.tick(400);
      assert(s.factory==bytes && b.counters.recovered_preambles==0 && b.counters.main_frames==0);
    }
    Sink s;haier_inline::Bridge b(s);init(b);s.factory.clear();s.local.clear();
    assert(b.send(poll.data(),poll.size(),6000));
    auto bytes=status();bytes.insert(bytes.begin(),0xFF);feed(b,false,bytes,6100);
    b.local_finished(6100);assert(s.local==status() && s.factory.empty());
    assert(b.counters.transaction_timeouts==0 && b.counters.recovered_preambles==1);
  }
  { // Real keep-alive reply with a missing delimiter: recover only on main RX
    // and only after both checksum and CRC prove the entire body is intact.
    const Bytes reply{0xFF,0xFF,0x0E,0x40,0,0,0,0,0,0xFD,0,0,0,0,0,0,0x4B,0xC1,0xDD};
    Sink s; haier_inline::Bridge b(s); b.start(0);
    feed(b, false, reply, 100);
    assert(b.counters.main_frames == 1 && b.counters.invalid == 0);
    Bytes truncated(reply.begin()+1,reply.end());
    feed(b, false, truncated, 400);
    assert(b.counters.main_frames == 2 && b.counters.recovered_preambles == 1 && b.counters.invalid == 0);
    feed(b, false, reply, 700);
    assert(b.counters.main_frames == 3);
    Bytes expected=reply;expected.insert(expected.end(),reply.begin(),reply.end());
    expected.insert(expected.end(),reply.begin(),reply.end());
    assert(s.factory==expected);
    auto damaged=truncated; damaged.back()^=1;
    feed(b,false,damaged,1000);assert(b.counters.main_frames==3 && b.counters.invalid>0);
    assert(b.counters.recovered_preambles==1);
    feed(b,true,truncated,1400);assert(b.counters.factory_frames==0);
    auto no_crc=wire(0xFD,false);no_crc.erase(no_crc.begin());
    feed(b,false,no_crc,1800);assert(b.counters.main_frames==3);
    auto missing_body=truncated;missing_body.erase(missing_body.begin()+5);
    feed(b,false,missing_body,2200);b.tick(2500);
    assert(b.counters.main_frames==3 && b.counters.recovered_preambles==1);
  }
  { // A real appliance may enable CRC in its negotiation answer immediately.
    Sink s; haier_inline::Bridge b(s); b.start(0);
    auto hello = wire(0x61, false, {0, 7});
    assert(b.send(hello.data(), hello.size(), 5000));
    auto version = wire(0x62, true, Bytes(38));
    feed(b, false, version, 5100);
    assert(s.local == version && s.factory.empty());
    b.local_finished(5100);
    assert(b.ready(5200));
  }
  { // Repaired factory keep-alive completes its pending transaction; a repaired
    // local status goes only to HaierProtocol, never to the factory module.
    Sink s; haier_inline::Bridge b(s); init(b); s.factory.clear(); s.local.clear();
    feed(b,true,wire(0xFC),6000);
    auto reply=wire(0xFD,true,Bytes(6));auto short_reply=reply;short_reply.erase(short_reply.begin());
    feed(b,false,short_reply,6100);
    assert(s.factory==reply && b.ready(6200));
    assert(b.send(poll.data(),poll.size(),6300));
    auto full_status=status();auto short_status=full_status;short_status.erase(short_status.begin());
    feed(b,false,short_status,6400);b.local_finished(6400);b.tick(9000);
    assert(s.local==full_status && s.factory==reply);
    assert(b.counters.recovered_preambles==2 && b.counters.transaction_timeouts==0);
  }
  { // No factory module: preserve direct request/response and unsolicited notifications.
    Sink s; haier_inline::Bridge b(s); b.start(0);
    assert(b.send(poll.data(), poll.size(), 5000));
    feed(b, false, status(), 5100);
    assert(s.local == status() && s.factory.empty());
    b.local_finished(5100);
    auto alarm = wire(4, true, Bytes(10));
    feed(b, false, alarm, 5200);
    assert(b.ready(5300));
    auto ack = wire(5);
    assert(b.send(ack.data(), ack.size(), 5300));
    assert(!b.factory_present());
  }
  { // Factory wins arbitration; own reply remains private; queued factory request is released.
    Sink s; haier_inline::Bridge b(s); init(b);
    s.main.clear(); s.factory.clear();
    assert(b.send(poll.data(), poll.size(), 5000));
    feed(b, true, poll, 5030);
    assert(s.main == poll);
    feed(b, false, status(), 5100);
    assert(s.local == status() && s.factory.empty());
    b.local_finished(5100);
    Bytes twice = poll; twice.insert(twice.end(), poll.begin(), poll.end());
    assert(s.main == twice && !b.ready(5300));
    feed(b, false, status(), 5200);
    assert(s.factory == status() && b.ready(5300));
    assert(b.counters.local_replies == 1);
  }
  { // Unknown and malformed packets remain byte-for-byte intact.
    Sink s; haier_inline::Bridge b(s); init(b);
    s.main.clear();
    auto unknown = wire(0x33, true, {0xFF, 0, 0xFF, 0x55});
    feed(b, true, unknown, 6000);
    assert(s.main == unknown && !b.ready(6500));
    auto corrupt = unknown; corrupt.back() ^= 1;
    feed(b, true, corrupt, 6600);
    Bytes both = unknown; both.insert(both.end(), corrupt.begin(), corrupt.end());
    assert(s.main == both && b.counters.invalid == 1);
  }
  { // Partial frame timeout is transparent and pauses injection.
    Sink s; haier_inline::Bridge b(s); init(b); s.main.clear();
    Bytes partial{0xFF, 0xFF, 10, 0x40};
    feed(b, true, partial, 6000);
    assert(!b.ready(6199)); b.tick(6200);
    assert(s.main == partial && b.counters.partial_timeouts == 1);
  }
  { // Late reply is discarded before held factory request can reach the appliance.
    Sink s; haier_inline::Bridge b(s); init(b); s.factory.clear(); s.main.clear();
    assert(b.send(poll.data(), poll.size(), 5000));
    feed(b, true, poll, 5500);
    b.local_finished(6100);
    feed(b, false, status(), 6500);
    assert(s.factory.empty() && s.local.empty() && s.main == poll);
    b.tick(8100);
    assert(s.main.size() == poll.size() * 2);
    feed(b, false, status(), 8200);
    assert(s.factory == status());
  }
  { // A later line error must not turn a completed local poll into a stale-reply filter.
    for (bool timeout : {false, true}) {
      Sink s; haier_inline::Bridge b(s); init(b);
      assert(b.send(poll.data(), poll.size(), 5000));
      if (!timeout) feed(b, false, status(), 5100);
      b.local_finished(5200);
      b.tick(7300);  // Any late local reply quarantine has expired.
      feed(b, true, poll, 7400);
      s.factory.clear();
      feed(b, false, Bytes{0x42}, 7450); // Noise before a valid factory response.
      feed(b, false, status(), 7500);
      Bytes expected{0x42}; auto reply=status();
      expected.insert(expected.end(), reply.begin(), reply.end());
      assert(s.factory == expected);
      b.tick(9500);
      assert(b.ready(9600));
      assert(b.pending_timeouts[1] == 0);
    }
  }
  { // A main-board notification is acknowledged by factory, not stolen as own reply.
    Sink s; haier_inline::Bridge b(s); init(b); s.factory.clear();
    assert(b.send(poll.data(), poll.size(), 5000));
    auto alarm = wire(4, true, Bytes(10));
    feed(b, false, alarm, 5040);
    assert(s.factory == alarm && s.local.empty());
    auto ack = wire(5); feed(b, true, ack, 5050);
    feed(b, false, status(), 5100); b.local_finished(5100);
    assert(b.ready(5200));
  }
  { // Full-frame decoder accepts fragmentation and stuffing in checksum/CRC too.
    for (unsigned value = 0; value < 256; ++value) {
      auto bytes = wire(1, true, {uint8_t(value), 0xFF, 0x55});
      haier_inline::Frame f;
      for (size_t i = 0; i < bytes.size(); ++i)
        assert(f.put(bytes[i], uint32_t(i)) == (i+1 == bytes.size() ? 1 : 0));
      assert(f.payload()[0] == value && f.payload()[1] == 0xFF);
    }
  }
  { // Overflow never discards factory bytes and disables further local injection.
    Sink s; haier_inline::Bridge b(s); init(b); s.main.clear();
    assert(b.send(poll.data(), poll.size(), 5000));
    Bytes burst(2200, 0x42); feed(b, true, burst, 5050);
    assert(b.passthrough_only() && b.counters.overflows == 1);
    assert(s.main.size() == poll.size() + burst.size());
    assert(std::equal(burst.begin(), burst.end(), s.main.begin() + poll.size()));
  }
  { // Factory startup can overlap the first direct poll.
    Sink s; haier_inline::Bridge b(s); b.start(0);
    assert(b.send(poll.data(), poll.size(), 5000));
    auto hello = wire(0x61, false, {0, 7}); feed(b, true, hello, 5010);
    feed(b, false, status(), 5050); b.local_finished(5050);
    assert(b.factory_present() && !b.ready(6000));
    feed(b, false, wire(0x62, false, Bytes(38)), 5100);
    feed(b, true, poll, 5300); feed(b, false, status(), 5400);
    assert(b.ready(5500));
  }
  { // Silent factory module: return to direct mode, then re-detect it automatically.
    Sink s; haier_inline::Bridge b(s); init(b);
    b.tick(60099); assert(b.factory_present());
    b.tick(60100); assert(!b.factory_present() && b.ready(60100));
    assert(b.counters.factory_fallbacks == 1);
    feed(b, true, poll, 60200); assert(b.factory_present() && !b.ready(60300));
    feed(b, false, status(), 60300); assert(b.ready(60400));
  }
  { // Fallback cannot release a pending local/factory exchange early.
    Sink s; haier_inline::Bridge b(s); init(b);
    assert(b.send(poll.data(), poll.size(), 60000));
    b.tick(60200); assert(b.factory_present());
    feed(b, false, status(), 60300); b.local_finished(60300);
    b.tick(60301); assert(!b.factory_present());
  }
  { // Millis wrap does not freeze the bridge.
    Sink s; haier_inline::Bridge b(s); const uint32_t start = 0xFFFFF000;
    init(b, start); assert(b.ready(start + 6000));
  }
  { // An upgrade conversation must not receive local status polls.
    Sink s; haier_inline::Bridge b(s); init(b);
    feed(b, true, wire(0xE1), 6000); b.tick(10000);
    assert(b.passthrough_only() && !b.ready(20000));
  }
  puts("Haier inline bridge: direct, shared bus, corruption, timeout, overflow and wrap tests passed");
}
