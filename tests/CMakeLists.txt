include(FetchContent)

# doctest/json ship pre-3.5 cmake_minimum_required, which CMake >= 4 rejects; allow it
# for these pinned subprojects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Test-only deps, pinned (docs/BUILD.md). The core itself stays zero-dep.
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)
FetchContent_MakeAvailable(doctest nlohmann_json)

add_executable(mudraka_tests
  test_main.cpp
  test_decoder.cpp
  test_ring_buffer.cpp
  test_clock_model.cpp
  test_stream.cpp
  test_parity.cpp)
target_link_libraries(mudraka_tests
  PRIVATE mudraka_core doctest::doctest nlohmann_json::nlohmann_json)
target_compile_definitions(mudraka_tests
  PRIVATE MUDRAKA_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/fixtures")

add_test(NAME mudraka_tests COMMAND mudraka_tests)
