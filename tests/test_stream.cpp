// End-to-end: feed a real captured session through MudrakaStream and pull it back.
#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <vector>

#include "fixture_loader.hpp"
#include "mudraka/mudra_decoder.hpp"
#include "mudraka/stream.hpp"

using namespace mudraka;

namespace {
struct Dst3 {
  std::array<std::vector<int32_t>, 3> ch;
  std::array<int32_t*, 3> ptr;
  explicit Dst3(uint64_t n) {
    for (int c = 0; c < 3; ++c) {
      ch[c].assign(n, 0);
      ptr[c] = ch[c].data();
    }
  }
  int32_t* const* p() { return ptr.data(); }
};
}  // namespace

TEST_CASE("feed a session, pull it back, check stats") {
  const auto sess = fx::load("24bit_strong_contraction");
  const auto snc = sess.snc_rx();
  REQUIRE(!snc.empty());

  MudrakaConfig cfg;                 // 3ch, 834 Hz, ring 4 s
  MudrakaStream stream(cfg, std::make_unique<MudraDecoder>());

  for (const auto& f : snc) {
    auto r = stream.feed(sess.payload(f), f.len, f.t_mono_ns / 1e9);
    CHECK(r.status == DecodeStatus::ok);
  }

  const uint64_t expected = static_cast<uint64_t>(snc.size()) * 18;
  CHECK(stream.head() == expected);

  auto s = stream.stats();
  CHECK(s.total_written == expected);
  CHECK(s.malformed_frames == 0);
  REQUIRE(s.last_device_time_us.has_value());

  // Pull the latest 256 samples.
  Dst3 d(256);
  auto pr = stream.latest(d.p(), 256);
  CHECK(pr.written == 256);
}

TEST_CASE("ring overflow surfaces loss through the stream") {
  // Tiny ring (1 s @ a low nominal rate) so a long feed overflows.
  MudrakaConfig cfg;
  cfg.profile.nominal_rate_hz = 100.0;  // ring capacity = 100 samples
  cfg.ring_seconds = 1;
  MudrakaStream stream(cfg, std::make_unique<MudraDecoder>());

  const auto sess = fx::load("16bit_rest");
  const auto snc = sess.snc_rx();
  for (const auto& f : snc) stream.feed(sess.payload(f), f.len, f.t_mono_ns / 1e9);

  Dst3 d(200);
  auto pr = stream.pull(0, d.p(), 200);  // cursor 0 is long gone
  CHECK(pr.lost > 0);
  auto s = stream.stats();
  CHECK(s.total_overwritten > 0);
}

TEST_CASE("malformed frame increments the counter without killing the stream") {
  MudrakaConfig cfg;
  MudrakaStream stream(cfg, std::make_unique<MudraDecoder>());
  const uint8_t junk[5] = {1, 2, 3, 4, 5};
  auto r = stream.feed(junk, sizeof(junk), 0.0);
  CHECK(r.status == DecodeStatus::malformed);
  CHECK(stream.stats().malformed_frames == 1);

  // a good frame still works afterward
  const auto sess = fx::load("24bit_rest");
  const auto snc = sess.snc_rx();
  stream.feed(sess.payload(snc.front()), snc.front().len, 0.0);
  CHECK(stream.head() == 18);
}
