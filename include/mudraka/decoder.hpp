// IDecoder interface: device bytes -> int32 samples pushed to a sink. See docs/ARCHITECTURE.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "mudraka/sample_sink.hpp"

namespace mudraka {

enum class DecodeStatus {
  ok,
  malformed,  // frame did not conform to the layout; counted, not fatal
  partial,    // bytes held in the decoder carry buffer awaiting the next notification
};

// Trivially-copyable per-frame metadata, returned by value.
struct DecodeResult {
  uint32_t samples_written = 0;            // per channel, this frame
  std::optional<uint32_t> seq;             // device sequence number -- absent for Mudra SNC
  std::optional<uint32_t> device_time_us;  // Mudra SNC trailer (device-clock microseconds)
  DecodeStatus status = DecodeStatus::ok;
};

// Device-specific decode lives behind this interface; new devices = new IDecoder only.
struct IDecoder {
  virtual ~IDecoder() = default;

  // Decode one transport frame (one BLE notification payload) into `sink`.
  // `recv_time` is the host monotonic receive time, in seconds.
  virtual DecodeResult decode(const uint8_t* frame, std::size_t len, double recv_time,
                              SampleSink& sink) = 0;

  virtual uint32_t channels() const = 0;
};

}  // namespace mudraka
