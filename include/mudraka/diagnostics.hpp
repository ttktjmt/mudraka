// Pollable stream-health snapshot. Status-not-throw model. See docs/DIAGNOSTICS.md.
#pragma once

#include <cstdint>
#include <optional>

namespace mudraka {

struct Stats {
  uint64_t total_written = 0;       // cumulative samples pushed (per channel)
  uint64_t total_overwritten = 0;   // cumulative samples overwritten before being pulled
  uint64_t malformed_frames = 0;    // frames that failed to decode
  uint64_t gap_count = 0;           // device-timeline discontinuities detected
  std::optional<uint32_t> last_seq;
  std::optional<uint32_t> last_device_time_us;
  double estimated_rate_hz = 0.0;   // from the ClockModel regression
};

}  // namespace mudraka
