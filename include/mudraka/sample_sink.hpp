// Narrow write interface the decoder pushes decoded samples into. See docs/ARCHITECTURE.md.
#pragma once

#include <cstdint>

namespace mudraka {

// N-channel sink (N == StreamProfile.channels). For Mudra the sample order is
// [ulnar, median, radial]. Decoupling the decoder from the concrete RingBuffer makes
// the verification gate trivial (tests pass a capturing sink).
struct SampleSink {
  virtual ~SampleSink() = default;
  virtual void push(const int32_t* sample) = 0;  // sample points to `channels` int32 values
};

}  // namespace mudraka
