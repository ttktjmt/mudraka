#include <doctest/doctest.h>

#include "mudraka/clock_model.hpp"

using mudraka::ClockModel;

TEST_CASE("deterministic nominal-rate reconstruction") {
  ClockModel c(1000.0, /*drift=*/false);
  c.observe(0, 100.0);  // anchor t0=100 at index 0
  CHECK(c.timestamp(0) == doctest::Approx(100.0));
  CHECK(c.timestamp(1000) == doctest::Approx(101.0));  // +1000 samples @ 1000 Hz = +1 s
  CHECK(c.timestamp(500) == doctest::Approx(100.5));
  CHECK(c.estimated_rate_hz() == doctest::Approx(1000.0));
}

TEST_CASE("drift correction recovers the true rate by regression") {
  ClockModel c(1000.0, /*drift=*/true);  // nominal says 1000, reality is 900
  const double true_rate = 900.0;
  for (int i = 0; i <= 9000; i += 100) {
    c.observe(static_cast<uint64_t>(i), 50.0 + i / true_rate);
  }
  CHECK(c.estimated_rate_hz() == doctest::Approx(900.0).epsilon(0.01));
  // timestamps follow the measured slope, not the (wrong) nominal.
  CHECK(c.timestamp(900) == doctest::Approx(c.timestamp(0) + 1.0).epsilon(0.01));
}

TEST_CASE("unanchored clock returns 0") {
  ClockModel c(834.0, false);
  CHECK(c.timestamp(123) == doctest::Approx(0.0));
}
