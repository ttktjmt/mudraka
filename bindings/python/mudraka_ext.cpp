// nanobind bindings for mudraka (Python wheel). See docs/FFI_BINDINGS.md.
//
// Marshaling contract: feed() takes a read-only bytes view; pull/envelope write into
// caller-provided (channels x N) int32 numpy arrays (C-contiguous, so each row is a
// channel and dst[c] is contiguous). Native lifetime is RAII (GC'd Python object).
#include <memory>

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "mudraka/mudraka.hpp"

namespace nb = nanobind;
using namespace mudraka;

namespace {

// Build per-channel row pointers into a C-contiguous (channels, N) int32 array.
template <typename Arr>
std::vector<int32_t*> rows(Arr& a, uint32_t channels) {
  std::vector<int32_t*> p(channels);
  for (uint32_t c = 0; c < channels; ++c) p[c] = a.data() + static_cast<size_t>(c) * a.shape(1);
  return p;
}

using I32Array = nb::ndarray<int32_t, nb::ndim<2>, nb::c_contig, nb::device::cpu>;

}  // namespace

NB_MODULE(_core, m) {
  m.doc() = "mudraka — C++ sEMG engine for Mudra Link raw SNC decoding";

  nb::class_<StreamProfile>(m, "StreamProfile")
      .def(nb::init<>())
      .def_rw("channels", &StreamProfile::channels)
      .def_rw("channel_names", &StreamProfile::channel_names)
      .def_rw("nominal_rate_hz", &StreamProfile::nominal_rate_hz)
      .def_rw("sample_width_bits", &StreamProfile::sample_width_bits)
      .def_rw("scale", &StreamProfile::scale)
      .def_rw("unit", &StreamProfile::unit);

  nb::class_<MudrakaConfig>(m, "Config")
      .def(nb::init<>())
      .def_rw("profile", &MudrakaConfig::profile)
      .def_rw("ring_seconds", &MudrakaConfig::ring_seconds)
      .def_rw("enable_drift_correction", &MudrakaConfig::enable_drift_correction);

  nb::class_<Stats>(m, "Stats")
      .def_ro("total_written", &Stats::total_written)
      .def_ro("total_overwritten", &Stats::total_overwritten)
      .def_ro("malformed_frames", &Stats::malformed_frames)
      .def_ro("gap_count", &Stats::gap_count)
      .def_ro("last_seq", &Stats::last_seq)
      .def_ro("last_device_time_us", &Stats::last_device_time_us)
      .def_ro("estimated_rate_hz", &Stats::estimated_rate_hz);

  nb::class_<MudrakaStream>(m, "Stream")
      .def("__init__",
           [](MudrakaStream* self, const MudrakaConfig& cfg) {
             new (self) MudrakaStream(cfg, std::make_unique<MudraDecoder>());
           })
      // feed one BLE notification; returns samples written (0 if malformed).
      .def(
          "feed",
          [](MudrakaStream& s, nb::bytes data, double recv_time) {
            const auto* p = reinterpret_cast<const uint8_t*>(data.c_str());
            return s.feed(p, data.size(), recv_time).samples_written;
          },
          nb::arg("data"), nb::arg("recv_time"))
      // drain new samples since `cursor` into a (channels, N) int32 array.
      .def(
          "pull_into",
          [](MudrakaStream& s, uint64_t cursor, I32Array out) {
            auto r = rows(out, s.channels());
            auto pr = s.pull(cursor, r.data(), out.shape(1));
            return nb::make_tuple(pr.written, pr.next_cursor, pr.lost);
          },
          nb::arg("cursor"), nb::arg("out"))
      .def(
          "latest_into",
          [](MudrakaStream& s, I32Array out) {
            auto r = rows(out, s.channels());
            auto pr = s.latest(r.data(), out.shape(1));
            return nb::make_tuple(pr.written, pr.next_cursor, pr.lost);
          },
          nb::arg("out"))
      .def(
          "envelope_into",
          [](MudrakaStream& s, uint64_t from, uint64_t to, I32Array mins, I32Array maxs) {
            auto rmin = rows(mins, s.channels());
            auto rmax = rows(maxs, s.channels());
            return s.envelope(from, to, static_cast<uint32_t>(mins.shape(1)), rmin.data(),
                              rmax.data());
          },
          nb::arg("from"), nb::arg("to"), nb::arg("mins"), nb::arg("maxs"))
      .def("stats", &MudrakaStream::stats)
      .def("head", &MudrakaStream::head)
      .def("channels", &MudrakaStream::channels)
      .def("timestamp", &MudrakaStream::timestamp, nb::arg("sample_index"));

  m.attr("__version__") = "0.1.0";
}
