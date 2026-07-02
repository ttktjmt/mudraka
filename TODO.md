# TODO — mudraka

State: native core + Python (nanobind) built & verified against real captures; SNC
decode accepted for **retail Mudra Link** (~834 Hz / 16-bit, decoded directly — no
oracle). WASM binding written but unbuilt. See `README.md` / `docs/README.md`.

## Next (highest value first)

- [x] **Build & verify WASM** — done with Emscripten 6.0.1 (pinned in `docs/BUILD.md`).
      `emcmake cmake -S . -B build-wasm -DMUDRAKA_BUILD_WASM=ON -DMUDRAKA_BUILD_TESTS=OFF && cmake --build build-wasm`,
      then `node tools/verify_wasm.mjs [fixture]` feeds SNC frames + drains via `feed`/`pullInto`
      and asserts head==frames*18, malformed==0, non-zero signal. All 3 fixtures pass.
      (Needed `-sENVIRONMENT=…,node` in `bindings/wasm/CMakeLists.txt` so the artifact loads under Node.)
- [x] **Tri-target parity, automated** — `expected.jsonl` committed per fixture
      (per-notification golden, native = reference & sole generator via
      `MUDRAKA_REGEN_GOLDEN=1`). native `tests/test_parity.cpp`, python
      `python/tests/test_parity.py`, wasm `tools/verify_wasm.mjs` all assert bit-exact
      (snc_ts + samples). All pass. (`docs/TEST_STRATEGY.md`.)
- [x] **CI — test matrix** — `.github/workflows/ci.yml`: native (cmake+ctest, ubuntu+macos),
      python (pip install . + pytest, ubuntu+macos), wasm (setup-emsdk 6.0.1 + node parity,
      ubuntu), all on the committed fixture corpus. Mirrors the locally-green commands.
- [x] **CI — release automation (tag-driven)** — `.github/workflows/release.yml`: on a
      `v*` tag, gate on the test matrix (`ci.yml` via `workflow_call`), then publish.
      Version single-source = the git tag, stamped into `pyproject.toml`
      (`tools/stamp_version.py`) + `npm/package.json` (`npm version`). PyPI via
      cibuildwheel (ubuntu/macos/windows) + sdist → **Trusted Publishing** (OIDC).
      npm via emsdk 6.0.1 build → `npm publish --provenance`.
      **One-time setup still required (user):** PyPI Trusted Publisher for `mudraka` →
      this repo/`release.yml`; repo secret `NPM_TOKEN`. Then push a `vX.Y.Z` tag.

## Fixtures

- [ ] Capture the rest of the coverage matrix with `tools/capture_session.py`:
      `{16bit,24bit} × {nerve_ulnar, nerve_median, nerve_radial}` (have: strong_contraction + rest).
      (Note: 16/24bit decode identically — kept only for provenance.)
- [ ] Confirm channel order `[ulnar,median,radial]` (labeling only) — optional
      cross-check vs the friend's `tmp/teleop_*.h5` layer-B floats. `docs/DECODE_VERIFICATION.md`.

## Packaging / housekeeping

- [x] Repo `LICENSE` (Apache-2.0) + `pyproject.toml` license set.
- [x] npm `package.json` (`npm/`) wrapping the WASM build for mudra-web-viewer to
      consume; artifacts + README copied in at publish time (`npm pack` verified).

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
