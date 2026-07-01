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
| **2080 Hz** | Website spec — but of a **different product, Mudra Pro** (not Mudra Link); confirmed by vendor 2026-07-02 | N/A to Mudra Link |
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

### Resolved (2026-07-02, vendor reply)
**834 Hz is the retail Mudra Link's hard limit** — final, not gated by any license or
command. The **2080 Hz** figure on the website is the spec of a **different product,
Mudra Pro**, not Mudra Link. So there is no rate lever to chase and nothing to unlock.

- **Target now: retail Mudra Link @ ~834 Hz, 16-bit.** `SET_SAMPLE_TYPE` is a no-op here.
- **Mudra Pro (higher rate) is a future device**, supported when needed via a new
  `IDecoder` + config — no Pro-specific code today (see `CONTEXT.md`).
- 834 Hz is accepted for the current target; `nominal_rate` seed = 834, regression governs.

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
- **2026-07-02** — Vendor reply: 834 Hz is the retail Mudra Link hard limit (final);
  2080 Hz belongs to a separate product, **Mudra Pro**. No rate lever, no oracle/license.
  Target Mudra Link now; Pro later via a new `IDecoder`.
- **2026-06-29** — **Decision: proceed at 834 Hz** (build decoder + oracle against the
  known 16-bit layout; the rate-agnostic engine is robust to a later change) **while
  querying the vendor** about 2080 Hz / dev-kit access in parallel.
