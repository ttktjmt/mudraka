#include "mudraka/clock_model.hpp"

namespace mudraka {

ClockModel::ClockModel(double nominal_rate_hz, bool enable_drift_correction)
    : nominal_rate_(nominal_rate_hz > 0 ? nominal_rate_hz : 1.0),
      drift_(enable_drift_correction) {}

void ClockModel::observe(uint64_t sample_index, double recv_time) noexcept {
  if (!anchored_) {
    anchored_ = true;
    t0_ = recv_time;
    i0_ = sample_index;
  }
  const double x = static_cast<double>(sample_index);
  n_ += 1;
  sx_ += x;
  sy_ += recv_time;
  sxx_ += x * x;
  sxy_ += x * recv_time;
}

// Regression slope (seconds per sample); 0 if insufficient/degenerate.
static double slope(double n, double sx, double sy, double sxx, double sxy) noexcept {
  if (n < 2) return 0.0;
  const double denom = n * sxx - sx * sx;
  if (denom == 0.0) return 0.0;
  return (n * sxy - sx * sy) / denom;
}

double ClockModel::timestamp(uint64_t sample_index) const noexcept {
  if (!anchored_) return 0.0;
  if (drift_) {
    const double b = slope(n_, sx_, sy_, sxx_, sxy_);
    if (b > 0.0) {
      const double a = (sy_ - b * sx_) / n_;  // intercept
      return a + b * static_cast<double>(sample_index);
    }
  }
  return t0_ + static_cast<double>(static_cast<int64_t>(sample_index) -
                                   static_cast<int64_t>(i0_)) /
                   nominal_rate_;
}

double ClockModel::estimated_rate_hz() const noexcept {
  if (drift_) {
    const double b = slope(n_, sx_, sy_, sxx_, sxy_);
    if (b > 0.0) return 1.0 / b;
  }
  return nominal_rate_;
}

}  // namespace mudraka
