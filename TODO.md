# TODO — mudraka

State: native core + Python (nanobind) built & verified against real captures; SNC
decode accepted for **retail Mudra Link** (~834 Hz / 16-bit, decoded directly — no
oracle). WASM binding written but unbuilt. See `README.md` / `docs/README.md`.

## Next (highest value first)

- [ ] **Build & verify WASM** — needs Emscripten:
      `emcmake cmake -S . -B build-wasm -DMUDRAKA_BUILD_WASM=ON -DMUDRAKA_BUILD_TESTS=OFF && cmake --build build-wasm`,
      then run under Node and exercise `feed`/`pullInto` on a fixture (mirror the
      Python end-to-end check). Fix any embind marshaling issues (`bindings/wasm/`).
- [ ] **Tri-target parity, automated** — commit `expected.jsonl` = native reference
      decode per fixture; add python + wasm tests that assert bit-exact vs it.
      (`docs/TEST_STRATEGY.md`; native tests already pass.)
- [ ] **CI** — GitHub Actions: native (cmake+ctest), Python (cibuildwheel), WASM
      (emcc+node), all running the committed fixture corpus.

## Fixtures

- [ ] Capture the rest of the coverage matrix with `tools/capture_session.py`:
      `{16bit,24bit} × {nerve_ulnar, nerve_median, nerve_radial}` (have: strong_contraction + rest).
      (Note: 16/24bit decode identically — kept only for provenance.)
- [ ] Confirm channel order `[ulnar,median,radial]` (labeling only) — optional
      cross-check vs the friend's `tmp/teleop_*.h5` layer-B floats. `docs/DECODE_VERIFICATION.md`.

## Packaging / housekeeping

- [ ] Repo `LICENSE` + set `pyproject.toml` license (currently "TBD").
- [ ] npm `package.json` wrapping the WASM build for mudra-web-viewer to consume.

## Later / do not build yet

- [ ] **Mudra Pro** (higher-rate product): add only when hardware exists — a new
      `IDecoder` + config, nothing else (`docs/CONTEXT.md`). No Pro code now.
- [ ] RingBuffer concurrent-read overrun re-check — current code assumes the consumer
      keeps up (fine for the streaming use). Harden only if a real lagging-reader race
      shows up. (`src/ring_buffer.cpp`)

## Notes for the next session

- Downstream projects (mudra-lsl, mudra-web-viewer) are **separate repos**; this one
  is mudraka only. They consume the published wheel / npm package.
- Design is doc-driven: every decision lives in `docs/` (indexed by `docs/README.md`),
  one file per topic. Keep updating docs as decisions land.
