# Context — the three projects and mudraka's place

> **Responsibility of this file:** the big picture. What mudraka is, what it is
> deliberately **not**, how it relates to its two sibling projects, and the repo
> strategy. Read this first to understand boundaries before any other doc.

---

## The constellation

Three projects, developed in **three separate repositories**. mudraka is the
shared core; the other two are downstream consumers.

```
                ┌─────────────────────────────────────────┐
                │  mudraka  (this repo — C++17 engine)      │
                │  raw sEMG (SNC) bytes → int32 samples     │
                └───────────────┬───────────────┬──────────┘
            Emscripten→WASM→npm  │               │  nanobind→wheel→PyPI
                                 ▼               ▼
                ┌────────────────────────┐  ┌──────────────────────────┐
                │ mudra-web-viewer        │  │ mudra-lsl                 │
                │ Web Bluetooth + WASM    │  │ bleak (BLE) + pylsl       │
                │ live waveform viewer    │  │ LSL outlet publisher      │
                │ (Chromium only)         │  │ (muse-lsl / OpenBCI-style)│
                └────────────────────────┘  └──────────────────────────┘
```

| Project | Repo | Role | Status here |
|---------|------|------|-------------|
| **mudraka** | this one | C++ engine: decode + buffer + clock + output | **In scope** |
| **mudra-lsl** | separate | Python pkg, BLE via bleak, publishes LSL via pylsl | constraint only |
| **mudra-web-viewer** | separate | Web app, Web Bluetooth + mudraka(WASM) | constraint only |

The two siblings are treated here **only as constraints on mudraka's API**. Their
internals are not designed in this effort.

---

## mudraka's responsibility boundary

**In scope (mudraka owns):**
- The single entry point `feed(raw byte frame + receive time)` — transport-agnostic.
- Device-byte → **int32 sample** decode (layer A), behind `IDecoder` / `MudraDecoder`.
- `RingBuffer` (int32 SoA, pre-allocated, SPSC), `ClockModel`, `StreamProfile`.
- Outputs: `pull` (latest N samples) and `envelope` (min/max decimation for drawing).
- Three build targets from one C++ core: native / wasm / python.

**Explicitly NOT in scope (mudraka never does):**
- **No transport.** No BLE, no LSL, no Web Bluetooth — bytes come in via `feed()`.
- **No layer-B compute.** No normalization to −1..+1, no RMS, no frequency, no
  neural inference, no µV scaling baked in (µV is a *derived* view using per-channel
  `scale`; the int32 counts remain the source of truth — mudraka never rounds them).

## Device models

- **Mudra Link (retail)** — the current target. ~834 Hz, 16-bit SNC (vendor-confirmed
  hard limit). Decoded by `MudraDecoder` (see `SNC_PACKET_HYPOTHESIS.md`).
- **Mudra Pro** — a separate, higher-rate product (the website's 2080 Hz spec). **Not
  built now.** When needed it slots in as a **new `IDecoder` + config** — the engine
  (ring, clock, stream, bindings) is already device- and rate-agnostic, so no
  Pro-specific work exists today. This is the whole extension mechanism; nothing more.

## Distribution

One C++ core fans out to two published artifacts (this is the dependency mechanism
for the siblings — they consume published artifacts, not source):

- **npm** — Emscripten → WASM (consumed by mudra-web-viewer).
- **PyPI** — nanobind/pybind11 → wheel via cibuildwheel (consumed by mudra-lsl).

## Repo strategy

**Decision (2026-06-25): polyrepo.** Each of the three projects lives in its own
repository; **this repository's responsibility is mudraka only.** Siblings depend
on mudraka via its **published artifacts** (PyPI wheel / npm package), not via
source.

Rationale: three heterogeneous ecosystems (C++/CMake, Python/scikit-build +
cibuildwheel, JS/npm + Emscripten); dependency is via published artifacts, so a
monorepo's atomic-cross-change benefit doesn't apply; per-repo CI and release
cadence stay simple and independent. If a dev-time aggregation is ever wanted, a
meta-repo of git submodules can bundle the three without changing any of them.

## Agreed source layout (this repo)

```
mudraka/
  CMakeLists.txt
  include/mudraka/      # public headers
  src/                  # C++17 core (decoder, ring, clock, profile)
  bindings/wasm/        # embind glue → npm
  bindings/python/      # nanobind glue → wheel
  fixtures/snc/...      # recorded test fixtures (see CAPTURE_FIXTURE_FORMAT.md)
  tests/
  docs/                 # these documents
```

## Decision log

- **2026-06-25** — Polyrepo; this repo = mudraka only; siblings consume published
  artifacts. Source layout above adopted.
