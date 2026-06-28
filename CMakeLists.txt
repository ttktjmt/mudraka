# mudraka — one C++17 core, three targets (native / Python wheel / WASM npm).
# See docs/BUILD.md.
cmake_minimum_required(VERSION 3.20)
project(mudraka VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(MUDRAKA_BUILD_TESTS  "Build native tests"                ON)
option(MUDRAKA_BUILD_PYTHON "Build the nanobind Python module"  OFF)
option(MUDRAKA_BUILD_WASM   "Build the Emscripten/embind module" OFF)

# The single source of truth, reused by every target (this is what makes the
# native/python/wasm decode-parity guarantee meaningful).
add_library(mudraka_core
  src/mudra_decoder.cpp
  src/ring_buffer.cpp
  src/clock_model.cpp
  src/stream.cpp)
target_include_directories(mudraka_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mudraka_core PUBLIC cxx_std_17)
set_target_properties(mudraka_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MUDRAKA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(MUDRAKA_BUILD_PYTHON)
  add_subdirectory(bindings/python)
endif()
if(MUDRAKA_BUILD_WASM)
  add_subdirectory(bindings/wasm)
endif()
