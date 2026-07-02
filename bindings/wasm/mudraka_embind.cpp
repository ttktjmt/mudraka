// Embind bindings for mudraka (WASM npm package). See docs/FFI_BINDINGS.md.
//
// Marshaling: feed() copies one notification into the heap (Web Bluetooth hands JS an
// ArrayBuffer outside the WASM heap, so one small copy per notification is unavoidable).
// pull/envelope write into a caller-allocated WASM-heap region (passed as a pointer), so
// JS reads the result directly (zero-copy on the JS side). Indices use double (< 2^53).
#include <cstdint>
#include <memory>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "mudraka/mudraka.hpp"

using namespace emscripten;
using namespace mudraka;

namespace {

class StreamWrap {
 public:
  explicit StreamWrap(const MudrakaConfig& cfg)
      : ch_(cfg.profile.channels),
        s_(std::make_unique<MudrakaStream>(cfg, std::make_unique<MudraDecoder>())) {}

  // data: a JS Uint8Array (one notification payload).
  double feed(val data, double recv_time) {
    std::vector<uint8_t> buf = convertJSArrayToNumberVector<uint8_t>(data);
    return static_cast<double>(s_->feed(buf.data(), buf.size(), recv_time).samples_written);
  }

  // dst_ptr: pointer into the WASM heap to a channel-major (channels * max) int32 region.
  val pull_into(double cursor, double dst_ptr, double max) {
    auto* base = reinterpret_cast<int32_t*>(static_cast<uintptr_t>(dst_ptr));
    const auto m = static_cast<uint64_t>(max);
    std::vector<int32_t*> rows(ch_);
    for (uint32_t c = 0; c < ch_; ++c) rows[c] = base + static_cast<size_t>(c) * m;
    auto pr = s_->pull(static_cast<uint64_t>(cursor), rows.data(), m);
    val o = val::object();
    o.set("written", static_cast<double>(pr.written));
    o.set("next_cursor", static_cast<double>(pr.next_cursor));
    o.set("lost", static_cast<double>(pr.lost));
    return o;
  }

  double head() const { return static_cast<double>(s_->head()); }
  double channels() const { return ch_; }
  double estimated_rate_hz() const { return s_->stats().estimated_rate_hz; }
  double malformed_frames() const { return static_cast<double>(s_->stats().malformed_frames); }
  // Device-clock microseconds of the last decoded notification (-1 if none yet).
  double last_device_time_us() const {
    auto v = s_->stats().last_device_time_us;
    return v ? static_cast<double>(*v) : -1.0;
  }
  double total_overwritten() const { return static_cast<double>(s_->stats().total_overwritten); }
  double timestamp(double i) const { return s_->timestamp(static_cast<uint64_t>(i)); }

 private:
  uint32_t ch_;
  std::unique_ptr<MudrakaStream> s_;
};

MudrakaConfig make_config(uint32_t channels, double nominal_rate_hz, uint32_t ring_seconds) {
  MudrakaConfig c;
  c.profile.channels = channels;
  c.profile.nominal_rate_hz = nominal_rate_hz;
  c.ring_seconds = ring_seconds;
  return c;
}

}  // namespace

EMSCRIPTEN_BINDINGS(mudraka) {
  class_<MudrakaConfig>("Config").constructor<>();
  function("makeConfig", &make_config);

  class_<StreamWrap>("Stream")
      .constructor<const MudrakaConfig&>()
      .function("feed", &StreamWrap::feed)
      .function("pullInto", &StreamWrap::pull_into)
      .function("head", &StreamWrap::head)
      .function("channels", &StreamWrap::channels)
      .function("estimatedRateHz", &StreamWrap::estimated_rate_hz)
      .function("malformedFrames", &StreamWrap::malformed_frames)
      .function("lastDeviceTimeUs", &StreamWrap::last_device_time_us)
      .function("totalOverwritten", &StreamWrap::total_overwritten)
      .function("timestamp", &StreamWrap::timestamp);
}
