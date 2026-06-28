# ClockModel & sampling rate

> **Responsibility of this file:** how mudraka derives time, and the (open)
> question of the true SNC sample rate. The clock's tunables live in the central
> config (`PUBLIC_API.md`).

---

## Timebase model (decision 2026-06-25)

- **Sample index is the authority.** Time is a *derived* quantity, never the
  source of truth.
- **Default = deterministic nominal-rate reconstruction:** `t(i) = t0 + i /
  nominal_rate`, where `t0` is anchored to the host receive time of an early
  notification.
- **Drift correction is optional, OFF by default.** When enabled, a linear
  regression (or PLL) over `(sample_index, recv_time)` pairs estimates the
  **effective** rate and anchor and replaces the nominal reconstruction. This is
  how the *actual* device rate is obtained — the nominal value is only a seed.
- **Clock domain = host.** Output timestamps are in the host clock (so mudra-lsl /
  mudra-web-viewer align with the host). The per-notification raw `recv_time` is
  **retained**. The device-side `SNC_TS` is a separate, non-authoritative input.
- `nominal_rate` is a **config seed, not truth** — see below.

---

## The sampling-rate question (OPEN — viability checkpoint)

Three numbers, three layers — do not conflate them:

| Value | Source | Layer |
|-------|--------|-------|
| **2080 Hz** | Official dev-kit spec page (also 0.035 µV sensitivity, 6+2 DRL electrodes) | ADC / raw sampling rate (claimed) |
| **~1000 Hz** | companion-SDK protocol JSON / signal spec ("nominal") | layer-B effective `frequency` |
| **~852 Hz** | measured from `tmp/teleop_20260525_181328.h5` (official Python SDK) | layer B (values ∈ [−1, +1]) |
| **~834 Hz** | **measured from a raw `0xfff4` capture** (`fixtures/sessions/24bit_strong_contraction`, fw 6.0.11.5) | **layer A** (raw int16) |

**RESOLVED (2026-06-25): the raw rate is ≈ 834 Hz, not 2080 Hz.** A real raw capture
(see `SNC_PACKET_HYPOTHESIS.md`) shows 18 samples × 3ch int16 + a µs trailer per
notification, 50 004 samples / 59.95 s = **834.1 Hz**; the trailer timestamp agrees
(21620 µs / 18 ≈ 832.6 Hz). This is **layer A** (the raw stream mudraka decodes), so
it is the authoritative figure — and it matches the layer-B ~852 Hz (little/no
decimation between layers).

**Not a throughput artifact:** the notification is 112 B < MTU 140, so the device is
sending exactly what it sampled per connection interval — 834 Hz is a genuine
firmware output rate, **not** MTU-clipped. A larger MTU will not raise it.

### Viability
834 Hz is **below the ≥ ~2000 Hz** the project needs for useful EMG analysis. Before
any device-replacement decision, the **one remaining lever** is **"full API access" /
a license** possibly unlocking a higher rate (the friend's hypothesis; also why
24-bit did not engage). See `DECODE_VERIFICATION.md` item #2.

**Strengthened (2026-06-29):** `SET_SAMPLE_TYPE` is a no-op at this access level
(16-bit and 24-bit requests give byte-identical 834 Hz streams — see
`SNC_PACKET_HYPOTHESIS.md`). The device is **locked to 16-bit / 834 Hz without full
API access**, so the viability verdict hinges entirely on whether a license unlocks
the higher rate. This is now the single gating question for the device choice.

**No BLE lever for rate (2026-06-29):** the full reverse-engineered firmware command
set (53 opcodes) contains **no SNC sample-rate command** — only `SET_SAMPLE_TYPE`
(16/24-bit, a no-op for us) and `HID_FREQUENCY` (HID pointer, unrelated to SNC). So
the rate **cannot be raised by any BLE command we can send**. If 2080 Hz exists at
all, it is gated outside the known command set — i.e. a **vendor / dev-kit-program /
special-firmware** matter (business-level "full API access"), **not** something we
can unlock in software. The friend's *licensed* official SDK also yielded ~852 Hz,
though their tier may also have been below "full API". Treat 2080 Hz as
vendor-dependent and uncertain; **~834 Hz is what this firmware delivers.**

**Decision (2026-06-25):**
- `nominal_rate` seed updated to **834 Hz** (measured); regression still governs at
  runtime.
- Viability verdict is **pending the license/full-API lever**; if 834 Hz is the
  ceiling even with full access, reconsider the device.

## Decision log
- **2026-06-25** — Timebase: sample-index authority; default deterministic
  nominal-rate reconstruction; optional regression/PLL drift correction (off by
  default); host clock domain; raw `recv_time` retained.
- **2026-06-25** — `nominal_rate` seed = 2080 Hz (provisional, official); real rate
  is regression-measured. Raw `0xfff4` rate is the viability checkpoint, judged from
  a raw capture — not from the layer-B 852 Hz recording.
- **2026-06-25** — Raw capture resolves it: **~834 Hz** (layer A, fw 6.0.11.5), not
  throughput-limited. Seed updated to 834. Below the ≥2000 Hz target → viability
  pending the full-API/license lever before any device-replacement decision.
- **2026-06-29** — Width (16/24-bit) and rate (834/2080 Hz) are **independent axes**;
  24-bit ≠ 2080 Hz, and no BLE command sets the rate.
- **2026-06-29** — **Decision: proceed at 834 Hz** (build decoder + oracle against the
  known 16-bit layout; the rate-agnostic engine is robust to a later change) **while
  querying the vendor** about 2080 Hz / dev-kit access in parallel.
