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

### Runners (all read the same fixture corpus)
- **native** — doctest; reads fixtures via nlohmann/json; `decode == expected`.
- **python** — pytest against the **built wheel**, decoding via the nanobind API.
- **wasm** — the Emscripten output run under **Node.js**, decoding via the embind API.

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
