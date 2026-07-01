# SNC decode verification

> **Responsibility of this file:** how we establish that mudraka's SNC byte→sample
> decode is correct. The decode format itself is in `SNC_PACKET_HYPOTHESIS.md`.

---

## Strategy (updated 2026-07-02)

**mudraka decodes the raw SNC stream directly. No official MudraSDK library, license,
or oracle is used** — we do not need Wearable Devices to bless the decode.

Correctness therefore rests on **empirical evidence from real captures**, not on a
differential oracle:

1. **Structural proof** (see `SNC_PACKET_HYPOTHESIS.md`): de-interleaving the bytes as
   int16×3 yields smooth per-channel waveforms; strong contraction saturates at exactly
   ±int16 limits; the 4-byte trailer is a monotonic device-clock timestamp consistent
   with the notification interval. Wrong layouts fail all three.
2. **Lossless round-trip test**: decode → re-encode → equals the original bytes. Proves
   the decode is a faithful reinterpretation, not a lossy transform.
3. **Real-fixture tests** (`tests/`, run in CI): every committed session decodes to the
   expected structure (18 samples × 3ch per 112-byte notification, ~834 Hz).
4. **Cross-target parity** (`TEST_STRATEGY.md`): native == python == wasm on the same
   fixtures — the native decode is the reference golden.

This is the gate: the above already pass (native + Python verified). The decode is
**accepted for Mudra Link**.

## Remaining open item (minor)

- **Channel order** — we label the three channels `[ulnar, median, radial]` (per the
  spec); which physical electrode maps to which is a *labeling* assumption, not a decode
  correctness issue (the integer values are order-independent). Optional free
  cross-check: the friend's existing layer-B recording (`tmp/teleop_*.h5`, values
  ≈ raw/32768) can sanity-check channel identity — no device or lib needed. Confirm if
  channel identity ever matters downstream.

## What was dropped and why

The earlier plan (disassemble `handleSnc`; run the desktop `MudraSDK` lib to record
`SNC_NO_FACTOR` as a bit-exact oracle; obtain a license) is **retired** — mudraka
decodes directly, so none of it is needed. `tools/oracle_harness.py` was deleted.

## Decision log
- **2026-06-25** — (superseded) oracle via `SNC_NO_FACTOR` from the desktop MudraSDK lib.
- **2026-07-02** — Direct decode; no official lib / license / oracle. Correctness rests
  on empirical structure + lossless round-trip + real-fixture tests + tri-target parity.
  Decode accepted for Mudra Link.
