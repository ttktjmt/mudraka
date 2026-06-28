// Top-level public object: the single ingress (feed) and egress (pull/envelope/stats).
// Owns the decoder, ring, and clock. See docs/PUBLIC_API.md and docs/ARCHITECTURE.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "mudraka/clock_model.hpp"
#include "mudraka/config.hpp"
#include "mudraka/decoder.hpp"
#include "mudraka/diagnostics.hpp"
#include "mudraka/ring_buffer.hpp"

namespace mudraka {

// Strict SPSC: one producer thread calls feed(); one consumer thread calls
// pull()/envelope()/stats(). mudraka spawns no threads. Not reentrant.
class MudrakaStream {
 public:
  MudrakaStream(MudrakaConfig config, std::unique_ptr<IDecoder> decoder);

  // --- ingress (producer thread): one BLE notification per call ---
  DecodeResult feed(const uint8_t* frame, std::size_t len, double recv_time);

  // --- egress (consumer thread) ---
  struct PullResult {
    uint64_t written;
    uint64_t next_cursor;
    uint64_t lost;
  };
  // Drain new samples since `cursor` into caller-provided per-channel buffers.
  PullResult pull(uint64_t cursor, int32_t* const* dst, uint64_t max);

  // Latest-N helper over the primitive.
  PullResult latest(int32_t* const* dst, uint64_t n);

  // Min/max decimation over absolute [from, to) into `buckets` per-channel buckets.
  uint32_t envelope(uint64_t from, uint64_t to, uint32_t buckets, int32_t* const* mins,
                    int32_t* const* maxs);

  Stats stats() const;
  uint64_t head() const { return ring_.total_written(); }
  uint32_t channels() const { return config_.profile.channels; }
  const MudrakaConfig& config() const { return config_; }

  // Derived host-clock timestamp (seconds) for an absolute sample index.
  double timestamp(uint64_t sample_index) const { return clock_.timestamp(sample_index); }

 private:
  MudrakaConfig config_;
  std::unique_ptr<IDecoder> decoder_;
  RingBuffer ring_;
  ClockModel clock_;
  Stats stats_;
  bool have_last_dt_ = false;
  uint32_t last_device_time_us_ = 0;
};

}  // namespace mudraka
