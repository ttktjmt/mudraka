# SNC decode verification strategy

> **Responsibility of this file:** how mudraka establishes the SNC byte→sample
> decode format and *proves* its decoder is correct. It does **not** contain the
> decode format itself (that lives in `SNC_PACKET_HYPOTHESIS.md`, written once the
> disassembly lands) nor the public API.
>
> **Audience:** developers / coding agents working on `MudraDecoder`.

---

## 0. Why this is the project's first hard gate

The single fact that blocks all three projects is unknown: **how SNC raw bytes
(characteristic `0xfff4`) map to samples** — sample width (16/24-bit), channel
order, endianness, header/sequence bytes, samples-per-packet. It is **not
recoverable from any available source**: it is implemented in the closed native
`handleSnc` (`MudraDevice.java:120` declares `native void handleSnc(byte[])`), and
the in-repo reverse-engineering (`re/RE_METHODOLOGY.md`, `re/protocol/…`) only
covers the *host→device COMMAND opcodes*, never the device→host SNC decode.
Prodilink forwards SNC bytes raw and never decodes them.

**Decision (2026-06-24): decode correctness is a hard gate.** We may scaffold the
rest of the library against a provisional, swappable layout, but **no advanced
implementation proceeds until the decoder is proven correct** by the procedure
below. Development is otherwise hardware-independent and fixture-first (CI must run
deterministically with no device attached).

---

## 1. The layer trap — pick the right tap point

There are **two distinct layers** (see `MUDRA_LINK_SIGNAL_SPEC.md` §"raw bytes vs
computed signal"). The oracle must compare at the layer mudraka actually targets.

| Layer | What it is | mudraka target? |
|-------|-----------|-----------------|
| **A — raw counts** | the decoded integer samples, before scaling | **Yes** (mudraka holds int32 as truth) |
| **B — computed** | normalized `float` −1..+1 + RMS/frequency, post neural/scale | No |

The native lib exposes SNC two ways. **Only one is a valid oracle:**

- ❌ **Streaming callback** `onSncDataReady(ts, float[] data, freq, freqStd, float[] rms)`
  — `data` is `float[]` = **layer B**. Confirmed in
  `re/python_sdk/.../computation_wrapper.py` (`OnRawDataCallback` returns
  `List[float]`). **Not usable** as a layer-A oracle.
- ✅ **Data recorder** `start_recording(types[]) → get_json_recording()` — the
  `RecordingDataType` enum exposes **`SNC_NO_FACTOR1/2/3`** ("raw EMG before factor
  is applied — the rawest values") plus `SNC_TS` (device time) and `SNC_APP_TS`.
  `SNC_NO_FACTOR` is the **layer-A tap point** and is the oracle we use.

> ⚠️ **To verify empirically (can't be confirmed without running the lib):** whether
> `SNC_NO_FACTOR` is *bit-exactly* the decoded integer, or whether a small transform
> still sits between the byte decode and `SNC_NO_FACTOR`. If a known transform
> exists, the gate compares modulo that transform.

---

## 2. Two-pronged truth: derive **and** confirm

**Decision (2026-06-24):** use the native lib in two complementary roles.

- **(α) Source of truth — disassemble `handleSnc`.** Read the decode logic directly
  out of the native binary to obtain the exact layout (bit width per sample-type,
  channel order, endianness, header length, samples-per-packet). This yields the
  format *deterministically* rather than by guessing. The team already has ARM64
  disassembly tooling (demonstrated in `re/RE_METHODOLOGY.md`).
- **(β) Independent confirmation + regression gate — recorder differential.**
  Feed captured raw bytes through the native lib with recording enabled and assert
  **mudraka's int32 decode == recorder `SNC_NO_FACTOR`, bit-exact**, over N
  fixtures. This guards against (a) misreading the disassembly and (b) future
  regressions.

Rationale for both: the decode is the core deliverable. (β) alone risks a
false-positive layout (the search space {bit-width × channel-order × endianness ×
header × samples-per-packet} can have multiple layouts that coincidentally match a
small fixture set). (α) alone risks instruction misreads. Together they give
certainty.

---

## 3. Lightweight oracle harness (no Android, no emulator)

**Decision (2026-06-24): no fallback path.** We commit to running the **desktop
native build** of the compute library directly on the host.

A host-loadable desktop build exists: `re/python_sdk/.../mudra.py` loads the lib by
name **`MudraSDK`** via `load_library('MudraSDK', 'MudraSDK')` with the comment
"auto-detect `.dll/.so/.dylib`", matching the product's desktop OS support
(Windows/macOS/ChromeOS). So we use the **macOS `.dylib` / Linux `.so`** build —
**not** the Android `libMudraAndroid.so` (ARM64, would need an emulator).

The existing `re/python_sdk` `ComputationWrapper` already declares the ctypes
signatures for `create_computation` / `handle_data` / `start_recording` /
`get_json_recording`, so the oracle harness is thin.

```
[ONCE, on the real band]  capture raw SNC notification payloads (char 0xfff4)
        │                 → fixtures/snc_raw_*.bin   (device not needed after this)
        ▼
[OFFLINE, host + CI]      dlopen/ctypes MudraSDK.dylib
        create_computation → enable_recording
        → start_recording([SNC_NO_FACTOR1..3, SNC_TS])
        → handle_data(raw_bytes)  (replayed in capture order)
        → get_json_recording()
        ▼
[golden fixture]          snc_raw_*.bin  ↔  expected SNC_NO_FACTOR samples
        ▼
[GATE]                    mudraka int32 decode  ==  expected  (all fixtures, bit-exact)
                          → only then may advanced implementation proceed
```

The same golden fixtures later drive the **cross-target parity test**
(native == wasm == python decode), which is mudraka's permanent CI guard.

---

## 4. The gate condition (formal)

> mudraka's `MudraDecoder` reproduces the recorder's `SNC_NO_FACTOR1..3` (aligned by
> `SNC_TS`) **bit-exactly** for every captured fixture, for **both** 16-bit and
> 24-bit sample-type modes.

Until this holds, only scaffolding (API shape, RingBuffer, ClockModel, build,
bindings against a provisional layout) is built — not relied upon as correct.

---

## 5. Open items / action list

| # | Item | Owner | Blocks |
|---|------|-------|--------|
| 1 | Obtain the **desktop `MudraSDK`** shared lib (+ model files it needs at `create_computation`) | user | the oracle harness |
| 2 | Confirm the **license model** (device-specific / account-specific / other) and that `SNC_NO_FACTOR` recording works under the owned license. User purchased Mudra Link → license presumed held; details TBD. | user | running the oracle |
| 3 | Capture raw SNC fixtures from the real band (16-bit and 24-bit modes) — see `CAPTURE_FIXTURE_FORMAT.md` once defined | user + tooling | (α) and (β) |
| 4 | Confirm `SNC_NO_FACTOR` is bit-exact the decoded int (§1 warning) | empirical | gate definition |
| 5 | Disassemble `handleSnc` → write `SNC_PACKET_HYPOTHESIS.md` | dev | (α) |
| 6 | Confirm `SNC_NO_FACTOR` is emitted at the **raw decode rate** (1 per decoded sample), not decimated like the layer-B ~852 Hz output — else the (β) differential loses 1:1 alignment and we rely on (α). See `CLOCK_MODEL.md`. | empirical | (β) gate |

> **Oracle environment already exists.** `tmp/teleop_20260525_181328.h5` was produced
> by the official **Python SDK** (= the `re/python_sdk` ctypes path over the
> `MudraSDK` lib). The friend's working setup is exactly our oracle harness
> environment — a concrete source for the desktop lib + a license that already yields
> data (layer B). Whether `SNC_NO_FACTOR` recording needs a higher license tier
> (RawData) is still item #2.

## Decision log

- **2026-06-24** — Decode correctness is a hard gate; dev is fixture-first /
  hardware-independent.
- **2026-06-24** — Oracle = recorder `SNC_NO_FACTOR` (layer A), **not** the
  streaming float callback (layer B).
- **2026-06-24** — Two-pronged: (α) disassemble `handleSnc` as source of truth +
  (β) recorder differential as confirmation/regression gate.
- **2026-06-24** — Lightweight harness: host-load desktop `MudraSDK` and replay
  captured bytes offline. **No fallback** (no Android/emulator path).
