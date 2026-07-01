# SNC packet layout — hypothesis

> **Responsibility of this file:** the working hypothesis for the raw SNC byte
> layout (the single swap point `MudraDecoder` is built around). Verification of it
> lives in `DECODE_VERIFICATION.md`; the format on disk in `CAPTURE_FIXTURE_FORMAT.md`.
>
> **Status (2026-07-02): accepted layout for Mudra Link.** Empirically validated (see
> `DECODE_VERIFICATION.md`); mudraka decodes it directly — no official-lib oracle. Still
> the single swap point (`SncLayout`); a future Mudra Pro gets its own `IDecoder`.

Source: `fixtures/sessions/24bit_strong_contraction/` — fw `6.0.11.5`, macOS,
MTU 140, 2778 SNC notifications over ~60 s.

---

## Layout (accepted for Mudra Link)

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
- **Stable across conditions & requests:** `24bit_strong_contraction`, `16bit_rest`,
  `24bit_rest` all give the same 112-byte / 18-sample int16 layout at 834.7 Hz; the
  rest captures show smooth low-amplitude de-interleaved signals (layout holds
  unsaturated too).

## Notes / minor open
1. **16-bit is fixed on Mudra Link.** `SET_SAMPLE_TYPE` (`22 00`/`22 01`) is a no-op —
   both decode identically (int16, 18 samples, 834.7 Hz). Not a limitation to fight;
   just how the retail device streams.
2. **Rate ≈ 834 Hz is the Mudra Link limit** (vendor-confirmed 2026-07-02); the 2080 Hz
   figure is the separate **Mudra Pro** product. See `CLOCK_MODEL.md`.
3. **Channel order** `[ulnar, median, radial]` is a labeling assumption (values are
   order-independent). See `DECODE_VERIFICATION.md`.
4. **Trailer semantics**: device-clock µs (byte[3] currently 0) — assumption holds
   across captures; revisit only if rollover matters.

## Decision log
- **2026-06-25** — Provisional layout from real capture: 18×3 int16 LE interleaved
  + 4-byte LE µs trailer, no header; rate ≈ 834 Hz; 16-bit (24-bit not engaged).
  Pending oracle + disassembly confirmation.
- **2026-06-29** — `SET_SAMPLE_TYPE` confirmed **no-op** (16bit vs 24bit identical);
  layout stable across rest + contraction.
- **2026-07-02** — Layout **accepted for Mudra Link**; decoded directly (no oracle).
  834 Hz/16-bit is the retail device's spec; 2080 Hz is the separate Mudra Pro.
