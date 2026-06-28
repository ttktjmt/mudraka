// Mudra Link SNC decoder. The SncLayout struct is the SINGLE SWAP POINT documented in
// docs/SNC_PACKET_HYPOTHESIS.md -- correct these values once the oracle/disassembly confirms.
#pragma once

#include "mudraka/decoder.hpp"

namespace mudraka {

// Provisional layout, derived empirically from real captures (see SNC_PACKET_HYPOTHESIS.md):
//   112-byte notification = N samples x channels x int16 LE interleaved + 4-byte LE uint32
//   microsecond timestamp trailer. samples_per_notification is DERIVED from payload length
//   (not hardcoded -- it varies with connection interval / sample rate).
struct SncLayout {
  int bits_per_sample = 16;   // int16 LE, signed (24-bit also supported below)
  int channels = 3;           // interleaved [ulnar, median, radial]
  bool little_endian = true;
  int header_bytes = 0;
  int trailer_bytes = 4;      // uint32 LE device-clock microseconds
};

class MudraDecoder : public IDecoder {
 public:
  explicit MudraDecoder(SncLayout layout = {}) : layout_(layout) {}

  DecodeResult decode(const uint8_t* frame, std::size_t len, double recv_time,
                      SampleSink& sink) override;

  uint32_t channels() const override { return static_cast<uint32_t>(layout_.channels); }

  const SncLayout& layout() const { return layout_; }

 private:
  SncLayout layout_;
};

}  // namespace mudraka
