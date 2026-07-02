# TODO — mudraka

State: all three targets (native / Python wheel / WASM npm) built, verified **bit-exact**
against real captures, and **released** (`mudraka` 0.1.1 on PyPI + npm). CI test matrix +
tag/dispatch release automation are live. SNC decode accepted for **retail Mudra Link**
(~834 Hz / 16-bit, decoded directly — no oracle). See `README.md` / `docs/README.md`.

Everything left below is **hardware-gated** (needs a real band to capture) — no
code-only work remains.

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
      npm via emsdk 6.0.1 build → `npm publish` (OIDC Trusted Publishing, provenance auto).
      **One-time setup still required (user, both token-less OIDC):** PyPI Trusted Publisher
      (`mudraka` → this repo/`release.yml`/env `pypi`); npm — bootstrap-publish `mudraka`
      once locally, then add its Trusted Publisher. See `docs/BUILD.md`. Then push `vX.Y.Z`.

## Fixtures

- [ ] **(hardware)** Capture the rest of the coverage matrix with `tools/capture_session.py`:
      `{16bit,24bit} × {nerve_ulnar, nerve_median, nerve_radial}` (have: strong_contraction + rest).
      (Note: 16/24bit decode identically — kept only for provenance.)
- [x] Channel order `[ulnar,median,radial]` is a **labeling assumption** (the decode is
      order-independent, so this is not a correctness issue). The friend's-h5 cross-check is
      **dropped** (2026-07-02). Revisit only if a downstream consumer needs physical channel
      identity — then confirm with a purpose-recorded session in this repo's own capture
      format. `docs/DECODE_VERIFICATION.md`.

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
