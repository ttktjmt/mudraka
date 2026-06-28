# Capture & fixture format

> **Responsibility of this file:** the on-disk format of a recorded SNC test
> fixture, and how it is produced and consumed. It does **not** describe the decode
> format (`SNC_PACKET_HYPOTHESIS.md`) or the verification strategy that uses these
> fixtures (`DECODE_VERIFICATION.md`).

---

## Principle: fixtures are literal device output, full session

Test fixtures must be **as close as possible to what the real Mudra Link actually
transmits** — recorded once from the physical band and stored **untransformed**.

**Capture the entire communication session, not just SNC** (decision 2026-06-25):
every byte on every characteristic, **both directions** (device→host notifications
*and* host→device command writes), even traffic irrelevant to EMG. Reasons:
- **Emulate production faithfully** — the real environment runs multiple streams and
  control traffic concurrently; SNC framing/timing can depend on that context.
- **Full replay into the oracle** — the `MudraSDK` lib's `handle_data` may rely on
  state established by COMMAND/MESSAGE traffic; replaying the whole session (not just
  SNC) reproduces the real conditions.

- `capture.bin` holds the **exact bytes** seen on the wire (no hex, no re-encoding).
- `expected.*` is **not synthetic**: it is the output of the real `MudraSDK` compute
  library run over the captured SNC bytes (see `DECODE_VERIFICATION.md` §3).

Synthetic byte streams are **not** used as test fixtures.

---

## Layout — one capture = one directory

```
fixtures/sessions/<name>/     # <name> e.g. 24bit_strong_contraction
  capture.bin     # raw input  — exact wire bytes, all characteristics, both directions
  index.json      # framing    — per-event boundaries: uuid, direction, time
  meta.json       # conditions — how/what was captured (incl. negotiated MTU)
  expected.jsonl  # oracle out — generated offline from the SNC frames (the answer key)
```

(`fixtures/` sits at the mudraka package root — see `CONTEXT.md` source layout.)

Each file has a single responsibility: **input / framing / conditions / answer.**
SNC-only consumers (decode tests, oracle) filter `index.json` by the SNC UUID.

### `capture.bin`
The concatenation, **in arrival order**, of every event payload — all
characteristics, both directions. No length prefixes, no transformation. Boundaries
and identity live in `index.json` (kept separate so the `.bin` is byte-identical to
the wire payloads).

### `index.json`
Preserves per-event **boundaries, order, characteristic, direction, and time** —
insurance in case the native decoder is *stateful across notifications* (replays
must reproduce the exact sequence the device emitted).

```json
{
  "frames": [
    { "i": 0, "offset": 0,  "len": 3,  "uuid": "0000fff1-...", "dir": "tx", "t_mono_ns": ... },
    { "i": 1, "offset": 3,  "len": 40, "uuid": "0000fff4-...", "dir": "rx", "t_mono_ns": ... }
  ]
}
```
`offset`/`len` index into `capture.bin`; `uuid` is the characteristic; `dir` is
`rx` (notification) or `tx` (host write); `t_mono_ns` is the host monotonic time.

### `meta.json`
Capture conditions and provenance.
```json
{
  "sample_type": "24bit",        // or "16bit"  (22 01 / 22 00)
  "enabled_streams": ["snc"],    // streams enabled by tx writes during capture
  "mtu": 247,                    // ACTUAL negotiated MTU (may be < 247 on macOS)
  "fw_version": "x.y.z",
  "hand": "right",               // or "left"
  "serial": "...",
  "condition": "strong_contraction",  // rest | strong_contraction | nerve_ulnar | nerve_median | nerve_radial | mixed
  "platform": "darwin",          // capture host OS (affects MTU / IMU routing)
  "duration_s": 60.0,
  "n_frames": 1234,
  "frames_by_uuid": {"0000fff4-...": 1000, "0000fff1-...": 12},
  "capture_tool": "tools/capture_session.py",
  "capture_tool_version": "0.1",
  "captured_at": "2026-..-..T..:..Z"
}
```

> **macOS MTU caveat.** macOS cannot negotiate Android-style connection params, so
> the MTU may be < 247 and IMU may route via COMMAND `0xfff1` tag `0x70` instead of
> `0xfff5` (see `MUDRA_LINK_SIGNAL_SPEC.md` §5.3). Capturing on the **actual target
> platform** (the OS mudra-lsl will run on) is what "emulate production" means here.
> The per-sample SNC layout (bit width, channel order, endianness) is MTU-invariant;
> only framing (samples-per-notification) changes — which the per-notification `feed`
> + decoder carry buffer already handle.

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

## Capture tool — `tools/capture_session.py`

A self-contained **bleak** recorder (no Prodilink dependency). It:
- scans for a `mudra` device (or takes `--address`), connects, logs the negotiated MTU;
- subscribes to **all notify characteristics** (SNC, IMU, COMMAND, MESSAGE, LOGGING,
  BATTERY, CHARGING);
- sets the sample type (`22 00`=16bit / `22 01`=24bit) and enables the requested
  streams (default SNC `06 00 01`), **recording those tx writes too**;
- records every rx/tx event `(t_mono_ns, dir, uuid, bytes)` into `capture.bin` +
  `index.json`, and writes `meta.json` on exit (duration `--duration` or Ctrl-C).

Sample-type mapping confirmed from Prodilink (`device.py`: `{"16bit":0,"24bit":1}`).
Run it on the actual target OS to emulate production.

## Storage

Fixtures are **committed to the repo** (decision 2026-06-25, option A) so the
golden-driven tri-target tests run with no external fetch. One full session is
~0.8 MB; the full coverage matrix is a few MB — fine for git for now.

If the corpus grows large later, migrate to **git-lfs** or host the raw captures on
a platform like **Hugging Face** (datasets) and keep only the small `expected.jsonl`
goldens in-repo. Not needed yet.

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
- **2026-06-25** — Capture the **full session**: all characteristics, both
  directions (rx + tx), to emulate production and enable full replay into the oracle.
  `index.json` gains `uuid`/`dir`; fixtures move to `fixtures/sessions/<name>/`. Tool
  = self-contained `tools/capture_session.py` (bleak). Capture on the target OS.
- **2026-06-25** — Fixtures are **committed to the repo** (option A). If the corpus
  grows large, migrate to git-lfs or host raw captures on Hugging Face and keep only
  the small goldens in-repo.
