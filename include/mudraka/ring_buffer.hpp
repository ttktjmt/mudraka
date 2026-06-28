// Lock-free SPSC ring, int32 SoA, N channels, overwrite-oldest. See docs/ARCHITECTURE.md.
#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace mudraka {

// One producer pushes; one consumer reads. The producer never blocks: when full it
// overwrites the oldest samples. Loss is reported to the consumer (it must keep up).
class RingBuffer {
 public:
  RingBuffer(uint32_t channels, uint64_t capacity_samples);

  uint32_t channels() const noexcept { return channels_; }
  uint64_t capacity() const noexcept { return capacity_; }

  // Absolute count of samples ever pushed (also the head cursor).
  uint64_t total_written() const noexcept { return head_.load(std::memory_order_acquire); }

  // --- producer (single thread) ---
  void push(const int32_t* sample) noexcept;  // `sample` has `channels` values

  // --- consumer (single thread) ---
  struct ReadResult {
    uint64_t written;      // samples copied into dst
    uint64_t next_cursor;  // advance the caller's cursor to this
    uint64_t lost;         // samples overwritten before they could be read
  };
  // Copy up to `max` samples from absolute `cursor` into the caller's per-channel
  // buffers dst[0..channels) (each at least `max` long).
  ReadResult read(uint64_t cursor, int32_t* const* dst, uint64_t max) const noexcept;

  // Min/max decimation over absolute [from, to) into `buckets` per-channel buckets.
  // Returns the number of buckets actually filled.
  uint32_t envelope(uint64_t from, uint64_t to, uint32_t buckets, int32_t* const* mins,
                    int32_t* const* maxs) const noexcept;

 private:
  uint32_t channels_;
  uint64_t capacity_;
  std::vector<int32_t> buf_;        // channel-major SoA: buf_[c * capacity_ + slot]
  std::atomic<uint64_t> head_{0};   // total written (absolute)
};

}  // namespace mudraka
