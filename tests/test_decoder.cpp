// Decodes the committed real captures and checks the SNC layout hypothesis structurally.
// (Bit-exact semantic confirmation against the oracle SNC_NO_FACTOR is a separate step --
//  see docs/DECODE_VERIFICATION.md. These tests assert faithful, lossless reinterpretation.)
#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "fixture_loader.hpp"
#include "mudraka/mudra_decoder.hpp"

namespace {

struct CaptureSink final : mudraka::SampleSink {
  int ch;
  std::vector<int32_t> data;  // flat, `ch` values per sample
  explicit CaptureSink(int c) : ch(c) {}
  void push(const int32_t* s) override {
    for (int c = 0; c < ch; ++c) data.push_back(s[c]);
  }
  std::size_t samples() const { return data.size() / ch; }
};

}  // namespace

TEST_CASE("MudraDecoder decodes the strong-contraction capture") {
  const auto sess = fx::load("24bit_strong_contraction");
  const auto snc = sess.snc_rx();
  REQUIRE(snc.size() > 2000);

  mudraka::MudraDecoder dec;  // default SncLayout: 16-bit, 3ch, 4-byte trailer
  CaptureSink sink(3);
  uint64_t prev_dt = 0;
  bool have_prev = false;
  bool monotonic = true;
  uint64_t first_dt = 0, last_dt = 0;

  for (const auto& f : snc) {
    CHECK(f.len == 112);
    const auto r = dec.decode(sess.payload(f), f.len, f.t_mono_ns / 1e9, sink);
    REQUIRE(r.status == mudraka::DecodeStatus::ok);
    CHECK(r.samples_written == 18);  // (112 - 4) / (3 * 2)
    REQUIRE(r.device_time_us.has_value());
    const uint64_t dt = *r.device_time_us;
    if (have_prev && dt < prev_dt) monotonic = false;
    if (!have_prev) first_dt = dt;
    last_dt = dt;
    prev_dt = dt;
    have_prev = true;
  }

  // 18 samples per 112-byte notification.
  CHECK(sink.samples() == snc.size() * 18);

  // Strong contraction rails the int16 range -> confirms signed 16-bit.
  int sat = 0;
  for (int32_t v : sink.data)
    if (v == 32767 || v == -32768) ++sat;
  CHECK(sat > 1000);

  // Trailer behaves as a monotonic device-clock timestamp; implied rate ~834 Hz.
  CHECK(monotonic);
  const double secs = static_cast<double>(last_dt - first_dt) / 1e6;
  const double rate = static_cast<double>((snc.size() - 1) * 18) / secs;
  CHECK(rate > doctest::Approx(800.0));
  CHECK(rate < doctest::Approx(870.0));
}

TEST_CASE("decode is a lossless reinterpretation (round-trips to the bytes)") {
  const auto sess = fx::load("24bit_rest");
  const auto snc = sess.snc_rx();
  REQUIRE(!snc.empty());

  mudraka::MudraDecoder dec;
  const auto& f = snc.front();
  CaptureSink sink(3);
  const auto r = dec.decode(sess.payload(f), f.len, 0.0, sink);
  REQUIRE(r.status == mudraka::DecodeStatus::ok);

  // Re-encode the decoded int32 values as int16 LE and compare to the data region.
  const uint8_t* data = sess.payload(f);  // header_bytes == 0
  for (std::size_t i = 0; i < sink.data.size(); ++i) {
    const int16_t v = static_cast<int16_t>(sink.data[i]);
    CHECK(static_cast<uint8_t>(v & 0xff) == data[i * 2]);
    CHECK(static_cast<uint8_t>((v >> 8) & 0xff) == data[i * 2 + 1]);
  }
}

TEST_CASE("SET_SAMPLE_TYPE is a no-op: 16bit and 24bit requests decode identically") {
  // Both rest captures are int16/18-samples/834Hz regardless of the requested width.
  for (const char* name : {"16bit_rest", "24bit_rest"}) {
    const auto sess = fx::load(name);
    const auto snc = sess.snc_rx();
    REQUIRE(!snc.empty());
    mudraka::MudraDecoder dec;
    CaptureSink sink(3);
    for (const auto& f : snc) {
      CHECK(f.len == 112);
      const auto r = dec.decode(sess.payload(f), f.len, 0.0, sink);
      CHECK(r.status == mudraka::DecodeStatus::ok);
      CHECK(r.samples_written == 18);
    }
    CHECK(sink.samples() == snc.size() * 18);
  }
}

TEST_CASE("malformed frames are rejected, not crashed") {
  mudraka::MudraDecoder dec;
  CaptureSink sink(3);
  const uint8_t junk[7] = {0, 1, 2, 3, 4, 5, 6};  // 7-4=3, not a multiple of 6
  const auto r = dec.decode(junk, sizeof(junk), 0.0, sink);
  CHECK(r.status == mudraka::DecodeStatus::malformed);
  CHECK(r.samples_written == 0);
  CHECK(sink.samples() == 0);
}
