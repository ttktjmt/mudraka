# SNC packet layout — hypothesis

> **Responsibility of this file:** the working hypothesis for the raw SNC byte
> layout (the single swap point `MudraDecoder` is built around). Verification of it
> lives in `DECODE_VERIFICATION.md`; the format on disk in `CAPTURE_FIXTURE_FORMAT.md`.
>
> **Status (2026-06-25): empirically derived from a real capture** — strong evidence,
> but **not yet confirmed** against the oracle (`SNC_NO_FACTOR`) or a `handleSnc`
> disassembly. Treat as the provisional, swappable layout.

Source: `fixtures/sessions/24bit_strong_contraction/` — fw `6.0.11.5`, macOS,
MTU 140, 2778 SNC notifications over ~60 s.

---

## Layout (provisional)

A SNC notification on `0xfff4` is a fixed **112 bytes**:

```
 byte:  0 ───────────────────────────────── 107 | 108 ──────── 111
        18 samples × 3 ch × int16 LE (interleaved)| uint32 LE timestamp
        [u0 m0 r0][u1 m1 r1]...[u17 m17 r17]      | microseconds (device clock)
```

| Field | Value |
|-------|-------|
| Sample width | **int16** little-endian, signed |
| Channels | **3**, interleaved per sample: `[ulnar, median, radial]` |
| Samples / notification | **18** (here) — = `(payload − 4) / (channels × 2)` |
| Trailer | **4-byte LE uint32**, microsecond timestamp (device clock) |
| Header | **none** (data starts at byte 0) |
| Delivered rate | **≈ 834 Hz** |

`samples_per_notification` is **not fixed firmware** vs **not MTU-clipped**: 112 <
MTU 140, so the device sends exactly what it accumulated per connection interval
(~21.6 ms × 834 Hz ≈ 18). It will vary with the connection interval / sample rate,
**not** with MTU. The decoder must derive it from payload length, not hardcode 18.

### The swap point (`SncLayout`)
```cpp
struct SncLayout {
  int    bits_per_sample = 16;         // int16 LE (NOT the requested 24 — see anomaly)
  int    channels        = 3;          // interleaved [ulnar, median, radial]
  Endian endian          = Little;
  int    header_bytes    = 0;
  int    trailer_bytes   = 4;          // uint32 LE microsecond timestamp
  // samples_per_notification = (payload_len - header - trailer) / (channels * bits/8)
};
```

## Evidence
- **De-interleaving [0:108] as int16×3 yields smooth per-channel waveforms** (c0
  ramps −1,−5,−18,−46,−79…; c1/c2 likewise) — wrong layouts would not.
- **Saturation at exactly +32767 / −32768** during strong contraction across all
  three channels → confirms signed int16.
- **Trailer is a µs timestamp:** LE uint32, monotonic, median Δ = 21620 µs ≈ host
  notification interval (22070 µs). 21620 µs / 18 samples → 832.6 Hz.
- **Delivered rate:** 50 004 samples / 59.95 s = **834.1 Hz** (matches the trailer
  and the layer-B ~852 Hz from `tmp/teleop_*.h5`).

## Anomalies / open
1. **24-bit did not engage.** We sent `SET_SAMPLE_TYPE 22 01` (=24bit per Prodilink)
   yet received int16. Either the PP mapping is wrong, fw `6.0.11.5` ignores it, or
   24-bit is license-gated. (24-bit would mean 9 B/sample → 12 samples/notification →
   ~556 Hz: higher resolution, **lower** rate.)
2. **Rate ≈ 834 Hz, not the official 2080 Hz** — see `CLOCK_MODEL.md` viability. The
   open lever is "full API access" / a license unlocking a higher rate.
3. **Confirm against the oracle**: is `SNC_NO_FACTOR` bit-exact these int16 values,
   or is there a "factor" between them? (`DECODE_VERIFICATION.md` items #4/#6.)
4. **Trailer semantics**: assumed device-clock µs; confirm units/rollover (byte[3]
   currently 0).

## Decision log
- **2026-06-25** — Provisional layout from real capture: 18×3 int16 LE interleaved
  + 4-byte LE µs trailer, no header; rate ≈ 834 Hz; 16-bit (24-bit not engaged).
  Pending oracle + disassembly confirmation.
