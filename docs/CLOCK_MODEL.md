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
| **~852 Hz** | **measured** from `tmp/teleop_20260525_181328.h5` (official Python SDK recording) | **layer B** (values ∈ [−1, +1]) |

**Key finding:** the 852 Hz measurement is from the **official Python SDK = layer B
(post-compute, normalized floats)**. It does **not** bound the raw `0xfff4` wire
rate that mudraka decodes (layer A). The compute layer (NN/filtering) plausibly
decimates a faster raw stream down to ~850–1000 Hz. Bandwidth is not the limit
(2080 Hz × 3ch × 3B ≈ 19 KB/s, trivial for BLE).

**Decision (2026-06-25):**
- Seed `nominal_rate` provisionally at **2080 Hz** (official source); the **true rate
  is whatever the regression measures from a real raw capture**.
- The **true raw `0xfff4` rate is an open empirical question**, answered by the same
  raw capture we already need for decode (see `CAPTURE_FIXTURE_FORMAT.md`).
- That raw-rate measurement is the **project viability checkpoint**: if the raw rate
  is confirmed `< 2000 Hz`, *then* reconsider the device. **Do not judge viability
  from the layer-B 852 Hz figure** — it is the wrong layer.

> Why it matters: the user's goal needs ≥ ~2000 Hz for useful EMG analysis; the
> official spec claims 2080; the only observation so far is a layer-B 852. The raw
> capture resolves it.

## Decision log
- **2026-06-25** — Timebase: sample-index authority; default deterministic
  nominal-rate reconstruction; optional regression/PLL drift correction (off by
  default); host clock domain; raw `recv_time` retained.
- **2026-06-25** — `nominal_rate` seed = 2080 Hz (provisional, official); real rate
  is regression-measured. Raw `0xfff4` rate is the viability checkpoint, judged from
  a raw capture — not from the layer-B 852 Hz recording.
