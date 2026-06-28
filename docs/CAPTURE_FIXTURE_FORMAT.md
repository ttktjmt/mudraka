# Capture & fixture format

> **Responsibility of this file:** the on-disk format of a recorded SNC test
> fixture, and how it is produced and consumed. It does **not** describe the decode
> format (`SNC_PACKET_HYPOTHESIS.md`) or the verification strategy that uses these
> fixtures (`DECODE_VERIFICATION.md`).

---

## Principle: fixtures are literal device output

Test fixtures must be **as close as possible to what the real Mudra Link actually
transmits** — recorded once from the physical band and stored **untransformed**.

- `capture.bin` holds the **exact bytes** the device sent (no hex, no re-encoding).
- `expected.*` is **not synthetic**: it is the output of the real `MudraSDK`
  compute library run over those exact captured bytes (see
  `DECODE_VERIFICATION.md` §3).

Synthetic byte streams are **not** used as test fixtures.

---

## Layout — one capture = one directory

```
fixtures/snc/<name>/          # <name> e.g. 24bit_strong_contraction
  capture.bin     # raw input  — exact device bytes
  index.json      # framing    — notification boundaries + host receive time
  meta.json       # conditions — how/what was captured
  expected.jsonl  # oracle out — generated offline from capture.bin (the answer key)
```

(`fixtures/` sits at the mudraka package root; the exact root path is finalized by
the repo-layout decision — see `REPO_STRATEGY.md` when written.)

Each file has a single responsibility: **input / framing / conditions / answer.**

### `capture.bin`
The concatenation, **in arrival order**, of every BLE notification payload from
characteristic `0xfff4`. No length prefixes, no transformation — the bytes only.
Boundaries live in `index.json` (kept separate so the `.bin` is byte-identical to
the wire payloads).

### `index.json`
Preserves notification **boundaries, order, and receive time** — insurance in case
the native decoder is *stateful across notifications* (replays must reproduce the
exact sequence the device emitted).

```json
{
  "frames": [
    { "i": 0, "offset": 0,  "len": 40, "t_recv_ns": 173... },
    { "i": 1, "offset": 40, "len": 40, "t_recv_ns": 173... }
  ]
}
```
`offset`/`len` index into `capture.bin`; `t_recv_ns` is the host monotonic receive
time of that notification.

### `meta.json`
Capture conditions and provenance.
```json
{
  "sample_type": "24bit",        // or "16bit"
  "mtu": 247,
  "fw_version": "x.y.z",
  "hand": "right",               // or "left"
  "serial": "...",
  "condition": "strong_contraction",  // rest | strong_contraction | nerve_ulnar | nerve_median | nerve_radial | mixed
  "capture_tool": "mudraka-capture",
  "capture_tool_version": "0.x",
  "captured_at": "2026-..-..T..:..Z"
}
```

### `expected.jsonl`
Generated offline by the `MudraSDK` oracle harness — never hand-authored. One JSON
object per emitted SNC sample (aligned to `SNC_TS`):
```json
{ "snc_ts": 123456, "no_factor": [u, m, r] }
```
`no_factor` = recorder `SNC_NO_FACTOR1..3` (layer A). The gate asserts mudraka's
int32 decode of `capture.bin` equals this, bit-exact.

---

## Coverage matrix (what makes a sufficient fixture set)

Capture at minimum the cross-product:

`{16bit, 24bit} × {rest, strong_contraction, nerve_ulnar, nerve_median, nerve_radial}`

Purpose: exercise sign, full-scale amplitude, and per-channel separation so a
**wrong layout cannot coincidentally match** (closes the β false-positive risk in
`DECODE_VERIFICATION.md` §2).

---

## Capture tool

A thin subscriber (bleak directly, or Prodilink's `on_raw_snc`, which delivers one
notification payload per call) connects to the band, enables SNC (`06 00 01`), sets
sample type (`22 PP`), and writes `capture.bin` + `index.json` + `meta.json`. No
such tool exists in Prodilink today (only mapper/firmware dumps), so it is new but
small.

## Not part of the canonical fixtures

An nRF Sniffer / HCI snoop `pcap` is an **optional discovery cross-check** for
framing only. It is not a canonical test fixture (the app-layer per-notification
capture is).

## Decision log

- **2026-06-24** — Storage format = **raw `.bin` + `index.json`** (option A: most
  faithful to device bytes), not hex JSONL.
- **2026-06-24** — Per-capture split into `capture` / `index` / `meta` / `expected`
  (single responsibility per file).
- **2026-06-24** — Fixtures are real device recordings; `expected` is the real lib's
  output over them; no synthetic fixtures; sniffer pcap is non-canonical.
