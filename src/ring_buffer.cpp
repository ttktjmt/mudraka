#include "mudraka/ring_buffer.hpp"

#include <algorithm>

namespace mudraka {

RingBuffer::RingBuffer(uint32_t channels, uint64_t capacity_samples)
    : channels_(channels),
      capacity_(capacity_samples == 0 ? 1 : capacity_samples),
      buf_(static_cast<std::size_t>(channels) * capacity_) {}

void RingBuffer::push(const int32_t* sample) noexcept {
  const uint64_t idx = head_.load(std::memory_order_relaxed);  // single producer
  const uint64_t slot = idx % capacity_;
  for (uint32_t c = 0; c < channels_; ++c) {
    buf_[static_cast<std::size_t>(c) * capacity_ + slot] = sample[c];
  }
  head_.store(idx + 1, std::memory_order_release);
}

RingBuffer::ReadResult RingBuffer::read(uint64_t cursor, int32_t* const* dst,
                                        uint64_t max) const noexcept {
  const uint64_t h = head_.load(std::memory_order_acquire);
  const uint64_t oldest = h > capacity_ ? h - capacity_ : 0;
  const uint64_t lost = cursor < oldest ? oldest - cursor : 0;
  const uint64_t start = std::max(cursor, oldest);
  const uint64_t avail = h - start;
  const uint64_t n = std::min(avail, max);
  for (uint64_t k = 0; k < n; ++k) {
    const uint64_t slot = (start + k) % capacity_;
    for (uint32_t c = 0; c < channels_; ++c) {
      dst[c][k] = buf_[static_cast<std::size_t>(c) * capacity_ + slot];
    }
  }
  return {n, start + n, lost};
}

uint32_t RingBuffer::envelope(uint64_t from, uint64_t to, uint32_t buckets,
                              int32_t* const* mins, int32_t* const* maxs) const noexcept {
  const uint64_t h = head_.load(std::memory_order_acquire);
  const uint64_t oldest = h > capacity_ ? h - capacity_ : 0;
  from = std::max(from, oldest);
  to = std::min(to, h);
  if (buckets == 0 || to <= from) return 0;

  const uint64_t span = to - from;
  const uint32_t nb = static_cast<uint32_t>(std::min<uint64_t>(buckets, span));
  for (uint32_t b = 0; b < nb; ++b) {
    const uint64_t b0 = from + (span * b) / nb;
    const uint64_t b1 = from + (span * (b + 1)) / nb;
    for (uint32_t c = 0; c < channels_; ++c) {
      int32_t lo = INT32_MAX, hi = INT32_MIN;
      for (uint64_t i = b0; i < b1; ++i) {
        const int32_t v = buf_[static_cast<std::size_t>(c) * capacity_ + (i % capacity_)];
        lo = std::min(lo, v);
        hi = std::max(hi, v);
      }
      mins[c][b] = lo;
      maxs[c][b] = hi;
    }
  }
  return nb;
}

}  // namespace mudraka
