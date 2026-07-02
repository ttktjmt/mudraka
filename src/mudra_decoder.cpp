#include "mudraka/mudra_decoder.hpp"

namespace mudraka {
namespace {

// Read an unsigned little/big-endian integer of `bytes` width.
inline uint32_t read_u32(const uint8_t* p, int bytes, bool little) {
  uint32_t v = 0;
  if (little) {
    for (int i = 0; i < bytes; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
  } else {
    for (int i = 0; i < bytes; ++i) v = (v << 8) | p[i];
  }
  return v;
}

// Same, sign-extended to int32.
inline int32_t read_sample(const uint8_t* p, int bytes, bool little) {
  uint32_t v = read_u32(p, bytes, little);
  const int bits = bytes * 8;
  const uint32_t sign_bit = 1u << (bits - 1);
  if (v & sign_bit) v |= ~((1u << bits) - 1);  // sign-extend
  return static_cast<int32_t>(v);
}

}  // namespace

DecodeResult MudraDecoder::decode(const uint8_t* frame, std::size_t len, double /*recv_time*/,
                                  SampleSink& sink) {
  DecodeResult r;
  const int ch = layout_.channels;
  const int bps = layout_.bits_per_sample / 8;          // bytes per channel-sample
  const int stride = ch * bps;                          // bytes per full sample (all channels)
  const std::size_t overhead =
      static_cast<std::size_t>(layout_.header_bytes) + layout_.trailer_bytes;

  if (stride <= 0 || len < overhead || (len - overhead) % stride != 0) {
    r.status = DecodeStatus::malformed;
    return r;
  }

  const std::size_t data_len = len - overhead;
  const uint32_t n = static_cast<uint32_t>(data_len / stride);
  const uint8_t* data = frame + layout_.header_bytes;

  // Decode interleaved samples; reuse a small stack buffer (channels is tiny).
  int32_t sample[16];
  const int nch = ch <= 16 ? ch : 16;
  for (uint32_t s = 0; s < n; ++s) {
    const uint8_t* base = data + static_cast<std::size_t>(s) * stride;
    for (int c = 0; c < nch; ++c) {
      sample[c] = read_sample(base + c * bps, bps, layout_.little_endian);
    }
    sink.push(sample);
  }

  if (layout_.trailer_bytes > 0) {
    const uint8_t* tail = frame + layout_.header_bytes + data_len;
    const int tb = layout_.trailer_bytes >= 4 ? 4 : layout_.trailer_bytes;
    r.device_time_us = read_u32(tail, tb, layout_.little_endian);
  }
  r.samples_written = n;
  r.status = DecodeStatus::ok;
  return r;
}

}  // namespace mudraka
