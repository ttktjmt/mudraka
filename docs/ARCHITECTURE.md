# Architecture — internal modules & data flow

> **Responsibility of this file:** the internal module decomposition and how data
> flows between them. The *public* call contract is in `PUBLIC_API.md`; the decode
> format in `SNC_PACKET_HYPOTHESIS.md`.
>
> Status: **in progress.** Type sketches are contracts, not final declarations.

---

## Modules

| Module | Role |
|--------|------|
| `MudrakaStream` (working name) | Top-level public object. Owns everything below. `feed()`/`pull()`/`envelope()` live here. |
| `IDecoder` / `MudraDecoder` | Device-specific byte→int32 decode, isolated behind an interface. `MudraDecoder` implements 16/24-bit. New devices = new `IDecoder` only. |
| `RingBuffer` | int32 SoA, **N** per-channel contiguous arrays (N = `StreamProfile.channels`), pre-allocated, SPSC. |
| `ClockModel` | Derives time from sample index (authority) + receive times. |
| `StreamProfile` | ch count / nominal rate / sample width / per-channel scale / unit. |
| `SampleSink` | Narrow write interface the decoder pushes decoded samples into. |

`MudrakaStream` owns the `IDecoder` (pluggable), the `RingBuffer`, the
`ClockModel`, and the `StreamProfile`. The decoder knows nothing about the buffer
implementation — only `SampleSink`.

---

## Data flow

```
feed(frame, recv_time)                       ── PUBLIC_API.md: 1 notification/call
    │
    ▼
IDecoder::decode(frame, recv_time, sink) ─────► sink.push(u, m, r)  // int32, zero-copy
    │                                                  │
    │ returns DecodeResult                             ▼
    │   { samples_written, optional<seq>, status }   RingBuffer (int32 SoA, pre-alloc)
    ▼
MudrakaStream / ClockModel
    • advance sample index (authority)
    • record recv_time anchor
    • gap detection from seq discontinuity  (device-independent — lives here, not in decoder)
```

### The decoder→buffer seam (decision 2026-06-25)

**Push-to-sink**, not return-by-value:

```cpp
// N-channel (StreamProfile.channels); Mudra order = [ulnar, median, radial]
struct SampleSink {
  virtual void push(const int32_t* sample /* length == channels */) = 0;
};

enum class DecodeStatus { ok, malformed, partial /* carry held */ };

struct DecodeResult {                 // trivially-copyable, returned by value
  uint32_t              samples_written;  // per channel, this frame
  std::optional<uint32_t> seq;            // device sequence number — only if the layout has one
  DecodeStatus          status;
};

DecodeResult IDecoder::decode(std::span<const uint8_t> frame,
                              RecvTime recv_time,
                              SampleSink& sink);
```

**Rationale:**
- Hot path is hundreds of notifications/sec × N samples × 3ch. Push-to-sink avoids
  per-frame heap allocation and copies straight into the pre-allocated ring. The
  virtual call is per-*notification*, not per-sample, so it's negligible.
- A `SampleSink` abstraction (not the concrete `RingBuffer`) decouples the decoder
  from the buffer and makes the verification gate trivial: tests pass a capturing
  sink that collects into vectors and compares to `expected.jsonl`, with no ring.
- Device sequence number is surfaced via `DecodeResult::seq` (`std::optional`),
  satisfying the brief's requirement while keeping it absent when the layout has none.

### Where responsibilities live
- **Sequence/gap detection:** in `MudrakaStream`/`ClockModel`, computed from
  `DecodeResult::seq` discontinuities — kept **out** of the decoder so decoders stay
  purely device-byte concerns.
- **Cross-notification fragmentation** (if captures prove it): an internal carry
  buffer **inside `MudraDecoder`** (`status = partial`); invisible to the public API.

### RingBuffer — overflow & SPSC (decision 2026-06-25)

**Overwrite-oldest, lock-free SPSC, producer never blocks.** When full, the oldest
samples are overwritten; the producer (the BLE-callback / transport thread driving
`feed`) is never stalled.

Loss is surfaced honestly via monotonic counters so consumers can detect it:
```cpp
uint64_t total_written;      // cumulative samples pushed (per channel)
uint64_t total_overwritten;  // cumulative samples overwritten before being pulled
```
- Capacity is a tunable read from the central config (see `PUBLIC_API.md` →
  Configuration); default holds a few seconds at nominal rate, pre-allocated, to
  absorb scheduler jitter.
- `pull` (latest N) and `envelope` (min/max decimation) are latest-window oriented,
  consistent with overwrite-oldest.
- **No-loss is out of mudraka's scope** — a consumer that must not drop (mudra-lsl)
  keeps up and/or sizes the ring, and reads `total_overwritten` to detect gaps.
  mudraka offers no blocking mode.

## Deferred (not built yet — no code-only work pending)
- **RingBuffer concurrent-read overrun re-check** — current code assumes the consumer
  keeps up (fine for the streaming use). Harden only if a real lagging-reader race shows
  up (`src/ring_buffer.cpp`).
- **Mudra Pro** (higher-rate product) — add only when hardware exists: a new `IDecoder`
  + config, nothing else (`CONTEXT.md`). No Pro code now.
- **Fixture coverage matrix** — capture `{16bit,24bit} × {nerve_ulnar, median, radial}`
  when a real band is available (`CAPTURE_FIXTURE_FORMAT.md`).

## Decision log
- **2026-06-25** — Decoder→buffer seam = push-to-`SampleSink` + small
  `DecodeResult` (`seq` as `std::optional`). Gap detection in engine/clock, not the
  decoder. Fragmentation handled by an internal carry buffer in `MudraDecoder`.
- **2026-06-25** — `RingBuffer` = overwrite-oldest, lock-free SPSC, producer never
  blocks; loss surfaced via `total_written`/`total_overwritten`. No-loss is the
  consumer's (mudra-lsl's) responsibility; mudraka has no blocking mode. Capacity is
  a central-config tunable (default a few seconds, pre-allocated).
- **2026-06-25** — Channels are **N-parameterized** (`StreamProfile.channels`,
  Mudra default 3); `SampleSink::push(const int32_t*)` is N-aware; ring holds N SoA
  arrays. (Decoder still writes 3 for Mudra; hot-path cost ~unchanged.)
