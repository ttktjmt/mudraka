# Public API — call contract

> **Responsibility of this file:** the public API surface callers see — entry/exit
> semantics, what the caller must guarantee, threading rules, and (later) the exact
> signatures and FFI boundary. Internal module decomposition lives in
> `ARCHITECTURE.md`; sample/decode format lives in `SNC_PACKET_HYPOTHESIS.md`.
>
> Status: **in progress** — being filled node by node. Signatures below are
> contracts, not yet final C++ declarations.

---

## Entry point: `feed()`

mudraka is **transport-agnostic**: the only ingress is `feed()`. The caller
(mudra-lsl via bleak, mudra-web-viewer via Web Bluetooth, tests via fixture replay)
delivers bytes; mudraka never touches a transport.

### Contract: one notification per call

**Decision (2026-06-25): `feed()` granularity = one BLE notification per call.**

```
feed(frame: bytes/span<const uint8_t>, recv_time)   // one GATT notification payload
```

- **One call = one transport frame** (one `0xfff4` notification payload), carrying
  its host receive time. A frame normally contains **N samples** (variable-length
  multi-sample, like IMU) — "one call = one frame", **not** "one call = one sample".
- The caller passes the notification payload **verbatim and in arrival order**; it
  must **not** concatenate or re-split frames.
- `recv_time` is the host monotonic receive time of *that* notification.

### Rationale
- Both real transports already deliver exactly one notification payload per event
  (bleak notification handler; Web Bluetooth `characteristicvaluechanged`), so frame
  boundaries exist for free; a byte-stream API would force re-concatenation then
  re-splitting — wasted work and lost boundary info.
- It matches the oracle harness, which replays `handle_data` per notification, so the
  verification gate compares identical framing (see `DECODE_VERIFICATION.md`).
- A general byte-stream re-sync layer cannot be designed responsibly while packet
  self-delimitation (length/sync/sequence) is unknown — that is part of the undecoded
  format. YAGNI until a capture proves it necessary.

### Consequence
- If captures later reveal a single logical sample **fragmented across two
  notifications**, that is absorbed by an internal carry buffer inside `MudraDecoder`
  (see `ARCHITECTURE.md`). **The public contract stays "one notification = one
  `feed()`"** regardless.

---

## Configuration — one central object

**Decision (2026-06-25): all tunables live in a single config object** passed at
construction, so settings change in one place (no knobs scattered across modules).
It aggregates the `StreamProfile` and per-module tunables. It **grows** as nodes
resolve; known fields so far:

```cpp
struct StreamProfile {                              // device/signal descriptor (N-channel)
  uint32_t                 channels        = 3;     // device-agnostic; Mudra = 3
  std::vector<std::string> channel_names   = {"ulnar","median","radial"};
  double                   nominal_rate_hz = 2080;  // SEED only; true rate is regression-measured
  uint8_t                  sample_width_bits = 24;  // 16 or 24 (SET_SAMPLE_TYPE); drives decode
  std::vector<double>      scale           = {0.035,0.035,0.035}; // µV/count — PROVISIONAL, unverified
  std::string              unit            = "uV";
};

struct MudrakaConfig {
  StreamProfile profile;
  uint32_t      ring_seconds = 4;              // RingBuffer capacity in seconds @ nominal rate (pre-alloc)
  bool          enable_drift_correction = false; // ClockModel: off = deterministic nominal reconstruction
  // + envelope defaults, ... (added as designed)
};
```

- `int32` counts are the source of truth; the µV view is the derived `count × scale`
  — **mudraka never rounds the counts**. `scale = 0.035` and `nominal_rate = 2080`
  are seeds to be confirmed against a real raw capture (see `CLOCK_MODEL.md`).

`RingBuffer`, `ClockModel`, etc. read their settings from this one object rather
than taking ad-hoc parameters.

## Exit: `pull` (cursor-based) + `envelope`

**Decision (2026-06-25): cursor-based read primitive, copy-out into caller-provided
buffers.**

`pull` cannot return a view into the ring — overwrite-oldest SPSC means the producer
may overwrite mid-read. So the **caller provides destination buffers** (numpy array
via nanobind, JS TypedArray via Embind) and mudraka copies ring→destination once (no
intermediate allocation across the FFI).

```
// primitive: drain new samples since the caller's cursor
PullResult pull(cursor, dst_u, dst_m, dst_r, max);
//   returns { written, new_cursor, lost }  where `lost` = samples overwritten
//   before they could be read (cursor fell behind the ring tail)
```

- **Cursor = absolute sample index** held by the consumer.
  - mudra-lsl drains sequentially (`lost` > 0 signals a gap to handle on its side).
  - mudra-web-viewer seeks the cursor to `head − N` for a latest-N window.
- **`latest(N)`** is a thin helper over the primitive (`cursor = head − N`).
- Integrates with `total_written` / `total_overwritten` (see `ARCHITECTURE.md`):
  loss is reported at read time → satisfies the drop/gap diagnostics requirement.

**`envelope` — drawing decimation, same range model.** Given a sample range and a
bucket count M (≈ pixel columns), returns per-channel per-bucket `(min, max)` into
caller-provided buffers. For waveform rendering without shipping every sample.

## Threading contract (decision 2026-06-25)

Callers must honor a strict **SPSC** contract; mudraka spawns **no threads of its
own**.

- **Exactly one producer thread** calls `feed`. **Exactly one consumer thread** calls
  `pull`/`envelope`. The two may run **concurrently on different threads** — that is
  the point of the lock-free ring.
- **No internal locks on the hot path.** Ring head/tail and the loss counters are
  atomics (acquire/release); `feed` never blocks.
- **Not reentrant.** Concurrent `feed` from two threads, or concurrent `pull` from
  two threads, is **undefined behavior by contract** — not defended against (perf
  over guard rails).
- **Lifecycle ops** (construct / destruct / reconfigure) must **not** run
  concurrently with `feed`/`pull`; the caller quiesces first.

Rationale: real-time sEMG wants a producer that never stalls; lock-free SPSC is the
standard. An internal-mutex (MPSC / call-from-anywhere) design was rejected — it adds
hot-path locking and contradicts the SPSC plan; determinism and throughput win.

## Pending (next nodes)

- `StreamProfile` fields & defaults; finalize `MudrakaConfig`.
- Build configuration (CMake layout, scikit-build-core, Emscripten toolchain, deps).

## Decision log
- **2026-06-25** — `feed()` = one BLE notification per call (frame + receive time),
  verbatim, in order; not a byte stream.
- **2026-06-25** — All tunables (ring capacity, profile, clock/envelope options)
  live in a single central `MudrakaConfig` passed at construction.
- **2026-06-25** — Exit = cursor-based `pull` primitive (+ `latest(N)` helper) and
  ranged min/max `envelope`; copy-out into caller-provided buffers (no ring views);
  loss reported via `PullResult::lost`.
- **2026-06-25** — Strict SPSC threading contract; mudraka spawns no threads; no
  hot-path locks; not reentrant (UB by contract); lifecycle ops require quiescence.
- **2026-06-25** — `StreamProfile` is N-channel (Mudra default 3) with
  channel_names / nominal_rate seed / sample_width / per-channel scale (0.035 µV/count
  provisional) / unit. `MudrakaConfig` = profile + ring_seconds + drift toggle.
