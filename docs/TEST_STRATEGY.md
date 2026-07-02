# Test strategy

> **Responsibility of this file:** how correctness is verified, especially decode
> parity across native / Python / WASM. Ties together the oracle
> (`DECODE_VERIFICATION.md`), fixtures (`CAPTURE_FIXTURE_FORMAT.md`), and CI
> (`BUILD.md`).

---

## Golden-driven tri-target parity (decision 2026-06-25)

The **native decode is the reference**; its output is committed as `expected.jsonl`,
the single golden answer key. All three implementations decode the same `capture.bin`
and must match it **bit-exact** — the permanent regression guard against the three
targets diverging. (Semantic correctness of the decode itself rests on the empirical
validation in `DECODE_VERIFICATION.md`, not an external oracle.)

```
expected.jsonl (golden = native reference decode)
        ▲ compare bit-exact
        │
   ┌────┴────┬─────────────┬──────────────┐
 native     python         wasm
 (doctest)  (pytest /      (Node.js /
            built wheel)    emscripten output)
   each decodes the SAME capture.bin and asserts == golden
```

### Runners (all read the same fixture corpus) — implemented 2026-07-02
- **native** — `tests/test_parity.cpp` (doctest); decodes via `MudraDecoder` and asserts
  `decode == expected`. **Also the generator**: `MUDRAKA_REGEN_GOLDEN=1` (re)writes the
  goldens (the single place the reference is produced). Run after an intentional decode
  change, then commit.
- **python** — `python/tests/test_parity.py` (pytest against the built wheel); feeds each
  notification via `Stream.feed`, checks `snc_ts` via `stats().last_device_time_us`, drains
  via `pull_into`, asserts bit-exact vs golden.
- **wasm** — `tools/verify_wasm.mjs` (Emscripten output under Node.js); same per-notification
  parity via the embind `feed`/`lastDeviceTimeUs`/`pullInto`.

Golden format: one JSON object per SNC notification, `{"snc_ts": µs, "samples": [[u,m,r]...]}`
(see `CAPTURE_FIXTURE_FORMAT.md`).

## Test depth (leverages "all targets link the same `mudraka_core`")
- **Core logic tested exhaustively in native** (doctest): ring overwrite + loss
  counters, cursor `pull` semantics, clock regression/reconstruction, `envelope`
  min/max, diagnostics.
- **Bindings get smoke + decode-parity only**: construct / `feed` / `pull` / `stats`
  round-trip across the FFI, plus golden parity. Logic is not re-tested per language
  (the shells are thin over one shared core) — binding tests exist to catch
  **marshaling** breakage and decode divergence.

## CI matrix
- Python: cibuildwheel platforms (manylinux / macOS / Windows, incl. arm64).
- WASM: Node.
- Native: per-OS.
- Every target runs the same committed fixture corpus.

## Decision log
- **2026-06-25** — Golden-driven tri-target bit-exact decode parity; core logic
  exhaustive in native, bindings smoke + parity only; CI matrix runs one shared
  fixture corpus.
- **2026-07-02** — Parity implemented for all three targets. Golden is **per
  notification** (not per sample) so every target verifies `snc_ts` too (via
  `stats().last_device_time_us`); native `test_parity.cpp` is the sole generator
  (`MUDRAKA_REGEN_GOLDEN=1`). `expected.jsonl` committed for all fixtures.
