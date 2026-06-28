// Loads a recorded session (capture.bin + index.json) for tests. See docs/CAPTURE_FIXTURE_FORMAT.md.
#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace fx {

constexpr const char* SNC = "0000fff4-0000-1000-8000-00805f9b34fb";

struct Frame {
  uint64_t offset = 0;
  uint64_t len = 0;
  std::string uuid;
  std::string dir;
  double t_mono_ns = 0.0;
};

struct Session {
  std::vector<uint8_t> blob;
  std::vector<Frame> frames;

  const uint8_t* payload(const Frame& f) const { return blob.data() + f.offset; }

  // SNC notification frames (device->host), in order.
  std::vector<Frame> snc_rx() const {
    std::vector<Frame> out;
    for (const auto& f : frames)
      if (f.dir == "rx" && f.uuid == SNC) out.push_back(f);
    return out;
  }
};

inline std::vector<uint8_t> read_bytes(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

inline Session load(const std::string& name) {
  const std::string dir = std::string(MUDRAKA_FIXTURE_DIR) + "/sessions/" + name;
  Session s;
  s.blob = read_bytes(dir + "/capture.bin");
  std::ifstream jf(dir + "/index.json");
  nlohmann::json j;
  jf >> j;
  for (const auto& fr : j.at("frames")) {
    Frame f;
    f.offset = fr.at("offset").get<uint64_t>();
    f.len = fr.at("len").get<uint64_t>();
    f.uuid = fr.at("uuid").get<std::string>();
    f.dir = fr.at("dir").get<std::string>();
    f.t_mono_ns = fr.value("t_mono_ns", 0.0);
    s.frames.push_back(std::move(f));
  }
  return s;
}

}  // namespace fx
