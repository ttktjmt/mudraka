#include "mudraka/stream.hpp"

#include <algorithm>

namespace mudraka {
namespace {

// Internal sink: forwards decoded samples straight into the ring (zero-copy).
struct RingSink final : SampleSink {
  explicit RingSink(RingBuffer& r) : ring(r) {}
  void push(const int32_t* sample) override { ring.push(sample); }
  RingBuffer& ring;
};

uint64_t ring_capacity(const MudrakaConfig& cfg) {
  const double cap = cfg.profile.nominal_rate_hz * static_cast<double>(cfg.ring_seconds);
  return cap < 1.0 ? 1 : static_cast<uint64_t>(cap);
}

}  // namespace

MudrakaStream::MudrakaStream(MudrakaConfig config, std::unique_ptr<IDecoder> decoder)
    : config_(std::move(config)),
      decoder_(std::move(decoder)),
      ring_(config_.profile.channels, ring_capacity(config_)),
      clock_(config_.profile.nominal_rate_hz, config_.enable_drift_correction) {}

DecodeResult MudrakaStream::feed(const uint8_t* frame, std::size_t len, double recv_time) {
  RingSink sink(ring_);
  const DecodeResult r = decoder_->decode(frame, len, recv_time, sink);

  if (r.status == DecodeStatus::malformed) {
    ++stats_.malformed_frames;
    return r;
  }
  if (r.samples_written > 0) {
    // Anchor/observe with the index of this frame's last sample.
    clock_.observe(ring_.total_written() - 1, recv_time);
  }

  if (r.seq) stats_.last_seq = r.seq;
  if (r.device_time_us) {
    const uint32_t dt = *r.device_time_us;
    if (have_last_dt_ && r.samples_written > 0) {
      const uint32_t delta = dt - last_device_time_us_;  // wrap-around safe (uint32)
      const double expected_us =
          static_cast<double>(r.samples_written) * 1e6 / config_.profile.nominal_rate_hz;
      if (delta > expected_us * 1.5) ++stats_.gap_count;
    }
    last_device_time_us_ = dt;
    have_last_dt_ = true;
    stats_.last_device_time_us = dt;
  }
  return r;
}

MudrakaStream::PullResult MudrakaStream::pull(uint64_t cursor, int32_t* const* dst, uint64_t max) {
  const RingBuffer::ReadResult rr = ring_.read(cursor, dst, max);
  stats_.total_overwritten += rr.lost;
  return {rr.written, rr.next_cursor, rr.lost};
}

MudrakaStream::PullResult MudrakaStream::latest(int32_t* const* dst, uint64_t n) {
  const uint64_t h = ring_.total_written();
  const uint64_t cursor = h > n ? h - n : 0;
  return pull(cursor, dst, n);
}

uint32_t MudrakaStream::envelope(uint64_t from, uint64_t to, uint32_t buckets,
                                 int32_t* const* mins, int32_t* const* maxs) {
  return ring_.envelope(from, to, buckets, mins, maxs);
}

Stats MudrakaStream::stats() const {
  Stats s = stats_;
  s.total_written = ring_.total_written();
  s.estimated_rate_hz = clock_.estimated_rate_hz();
  return s;
}

}  // namespace mudraka
