# Emscripten/embind module -> mudraka.js + mudraka.wasm for the npm package.
# Configure with the emscripten toolchain, e.g.:
#   emcmake cmake -S . -B build-wasm -DMUDRAKA_BUILD_WASM=ON -DMUDRAKA_BUILD_TESTS=OFF
#   cmake --build build-wasm
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "MUDRAKA_BUILD_WASM requires the Emscripten toolchain (emcmake).")
endif()

add_executable(mudraka_wasm mudraka_embind.cpp)
target_link_libraries(mudraka_wasm PRIVATE mudraka_core)
set_target_properties(mudraka_wasm PROPERTIES OUTPUT_NAME "mudraka")
target_link_options(mudraka_wasm PRIVATE
  --bind
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createMudraka"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sEXPORTED_RUNTIME_METHODS=['HEAP32','HEAPU8']"
  "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
  "-sENVIRONMENT=web,worker,node")  # node: so the same artifact runs the Node/CI parity check
