// Time derivation: sample-index is authority; time is derived. See docs/CLOCK_MODEL.md.
#pragma once

#include <cstdint>

namespace mudraka {

// Default: deterministic nominal-rate reconstruction t(i) = t0 + (i - i0) / rate, with
// t0 anchored to the first observed receive time. With drift correction enabled, an
// online linear regression of (sample_index, recv_time) supplies the effective rate.
class ClockModel {
 public:
  ClockModel(double nominal_rate_hz, bool enable_drift_correction);

  // Call once per fed frame: index of (e.g.) the frame's last sample + its receive time.
  void observe(uint64_t sample_index, double recv_time) noexcept;

  // Host-clock timestamp (seconds) for an absolute sample index.
  double timestamp(uint64_t sample_index) const noexcept;

  // Effective rate: regression slope if drift-correcting and enough points, else nominal.
  double estimated_rate_hz() const noexcept;

 private:
  double nominal_rate_;
  bool drift_;
  bool anchored_ = false;
  double t0_ = 0.0;
  uint64_t i0_ = 0;
  // online least-squares accumulators (x = sample_index, y = recv_time)
  double n_ = 0, sx_ = 0, sy_ = 0, sxx_ = 0, sxy_ = 0;
};

}  // namespace mudraka
