// Golden-driven tri-target parity (docs/TEST_STRATEGY.md). The native decode is the
// reference: each fixture's decode is committed as expected.jsonl and every target
// (native/python/wasm) must reproduce it bit-exact. One JSON object per SNC
// notification: {"snc_ts": <device_time_us>, "samples": [[u,m,r], ...]}.
//
// Regenerate the goldens (after an intentional decode change) with:
//   MUDRAKA_REGEN_GOLDEN=1 ctest --test-dir build   (or run the test binary directly)
#include <doctest/doctest.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fixture_loader.hpp"
#include "mudraka/mudra_decoder.hpp"

using nlohmann::json;

namespace {

// One notification's decode: the trailer timestamp + its samples (channel-tuples).
struct Note {
  uint64_t snc_ts;
  std::vector<std::vector<int32_t>> samples;
};

struct VecSink final : mudraka::SampleSink {
  int ch;
  std::vector<std::vector<int32_t>> rows;
  explicit VecSink(int c) : ch(c) {}
  void push(const int32_t* s) override { rows.emplace_back(s, s + ch); }
};

// Decode a fixture's SNC stream the reference way (decoder direct, no ring).
std::vector<Note> decode_reference(const std::string& name) {
  const auto sess = fx::load(name);
  mudraka::MudraDecoder dec;
  std::vector<Note> out;
  for (const auto& f : sess.snc_rx()) {
    VecSink sink(3);
    const auto r = dec.decode(sess.payload(f), f.len, f.t_mono_ns / 1e9, sink);
    REQUIRE(r.status == mudraka::DecodeStatus::ok);
    REQUIRE(r.device_time_us.has_value());
    out.push_back({*r.device_time_us, std::move(sink.rows)});
  }
  return out;
}

std::string golden_path(const std::string& name) {
  return std::string(MUDRAKA_FIXTURE_DIR) + "/sessions/" + name + "/expected.jsonl";
}

void write_golden(const std::string& name, const std::vector<Note>& notes) {
  std::ofstream f(golden_path(name));
  for (const auto& n : notes) {
    json j = {{"snc_ts", n.snc_ts}, {"samples", n.samples}};
    f << j.dump() << '\n';
  }
}

std::vector<Note> read_golden(const std::string& name) {
  std::ifstream f(golden_path(name));
  REQUIRE_MESSAGE(f.good(), "missing golden: " << golden_path(name)
                                               << " (regen with MUDRAKA_REGEN_GOLDEN=1)");
  std::vector<Note> out;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    json j = json::parse(line);
    out.push_back({j.at("snc_ts").get<uint64_t>(),
                   j.at("samples").get<std::vector<std::vector<int32_t>>>()});
  }
  return out;
}

const char* kFixtures[] = {"16bit_rest", "24bit_rest", "24bit_strong_contraction"};

}  // namespace

TEST_CASE("native decode matches the committed golden (bit-exact)") {
  const bool regen = std::getenv("MUDRAKA_REGEN_GOLDEN") != nullptr;
  for (const char* name : kFixtures) {
    const auto got = decode_reference(name);
    if (regen) {
      write_golden(name, got);
      MESSAGE("regenerated golden: " << name << " (" << got.size() << " notifications)");
      continue;
    }
    const auto want = read_golden(name);
    REQUIRE(got.size() == want.size());
    for (size_t i = 0; i < got.size(); ++i) {
      CHECK(got[i].snc_ts == want[i].snc_ts);
      REQUIRE(got[i].samples.size() == want[i].samples.size());
      CHECK(got[i].samples == want[i].samples);
    }
  }
}
