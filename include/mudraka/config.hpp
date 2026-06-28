// Central configuration for a mudraka stream. See docs/PUBLIC_API.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mudraka {

// Device/signal descriptor (N-channel). Defaults reflect the *observed* Mudra Link
// reality (16-bit, ~834 Hz) -- see docs/SNC_PACKET_HYPOTHESIS.md / docs/CLOCK_MODEL.md.
struct StreamProfile {
  uint32_t channels = 3;
  std::vector<std::string> channel_names = {"ulnar", "median", "radial"};
  double nominal_rate_hz = 834.0;        // SEED only; the regression governs at runtime
  uint8_t sample_width_bits = 16;        // observed; SET_SAMPLE_TYPE is a no-op on fw 6.0.11.5
  std::vector<double> scale = {0.035, 0.035, 0.035};  // uV/count -- PROVISIONAL, unverified
  std::string unit = "uV";
};

// One central, construction-time config; modules read their tunables from here.
struct MudrakaConfig {
  StreamProfile profile;
  uint32_t ring_seconds = 4;             // RingBuffer capacity in seconds @ nominal rate (pre-alloc)
  bool enable_drift_correction = false;  // ClockModel: off => deterministic nominal reconstruction
};

}  // namespace mudraka
