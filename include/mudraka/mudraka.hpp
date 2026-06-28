// Umbrella header for the mudraka C++ sEMG engine. See docs/README.md for the design.
#pragma once

#include "mudraka/clock_model.hpp"
#include "mudraka/config.hpp"
#include "mudraka/decoder.hpp"
#include "mudraka/diagnostics.hpp"
#include "mudraka/mudra_decoder.hpp"
#include "mudraka/ring_buffer.hpp"
#include "mudraka/sample_sink.hpp"
#include "mudraka/stream.hpp"

namespace mudraka {

inline constexpr int version_major = 0;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;

}  // namespace mudraka
