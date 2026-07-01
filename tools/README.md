# tools/ — host-side capture tooling

Python utilities for recording Mudra Link sessions into mudraka test fixtures.
**Not** part of the mudraka C++ core or its build; these run on a host with the
physical band. Fixture format: [`../docs/CAPTURE_FIXTURE_FORMAT.md`](../docs/CAPTURE_FIXTURE_FORMAT.md).

## Setup (once)
```sh
cd /Users/tatsuki/projects/mudraka
python3 -m venv .venv
. .venv/bin/activate              # Windows: .venv\Scripts\activate
pip install -r tools/requirements.txt
```

---

## `capture_session.py` — record a session

Records **all characteristics, both directions** (device→host notifications *and*
host→device command writes), so the capture emulates production. Run it on the
**actual target OS** — the negotiated MTU and IMU routing are platform-specific.

### Step by step

1. **Put the band on** and power it up (charged, not on the charger).
2. **Activate the venv** (`. .venv/bin/activate`).
3. **Run a capture**, choosing the sample width, the condition you'll perform, and a
   duration. Example — 24-bit, 60 s, strong contraction on the right hand:
   ```sh
   python tools/capture_session.py \
       --bits 24 \
       --condition strong_contraction \
       --hand right \
       --duration 60 \
       --out fixtures/sessions/24bit_strong_contraction
   ```
4. **Perform the condition** while it records (e.g. clench hard for the full 60 s).
   The band must already be streaming — the tool enables SNC for you.
5. **Wait for it to finish** (or press **Ctrl-C** to stop early — the files are still
   written cleanly).
6. **Verify immediately**:
   ```sh
   python tools/inspect_capture.py fixtures/sessions/24bit_strong_contraction
   ```

### What you'll see on the console
```
Scanning for 'mudra' (timeout 10.0s)...
Found Mudra L ... (XXXXXXXX-XXXX-...)        # CoreBluetooth UUID on macOS
Connecting to ...
Connected. MTU=247 fw=1.2.3
Subscribed to 7/7 characteristics.
sample_type=24bit  enabled=['snc']
Recording for 60s (Ctrl-C to stop early)...
Done. 1043 frames (1001 SNC) over 60.0s -> fixtures/sessions/24bit_strong_contraction/
```

### Output (in `--out`)
| File | Contents |
|------|----------|
| `capture.bin` | exact wire bytes — all characteristics, both directions, in order |
| `index.json`  | per-event `{i, offset, len, uuid, dir, t_mono_ns, t_wall}` |
| `meta.json`   | conditions + negotiated MTU, fw, serial, per-uuid counts |

### Options
| Flag | Default | Meaning |
|------|---------|---------|
| `--out DIR` | *(required)* | output fixture directory |
| `--bits {16,24}` | `24` | SNC sample width (sends `22 00` / `22 01`) |
| `--duration SEC` | until Ctrl-C | record length |
| `--condition NAME` | – | `rest` \| `strong_contraction` \| `nerve_ulnar` \| `nerve_median` \| `nerve_radial` \| `mixed` (metadata) |
| `--hand {left,right}` | – | which wrist (metadata) |
| `--enable STREAM` | `snc` | repeatable: `snc`, `imu_acc`, `imu_gyro`, `imu_quaternion`, `gestures`, `pressure`, `navigation` |
| `--address ADDR` | – | connect directly, skip scan (see macOS note) |
| `--name-filter STR` | `mudra` | device-name substring used when scanning |
| `--scan-timeout SEC` | `10` | scan timeout |
| `--post-connect-delay SEC` | `2` | settle time after connect before enabling streams |
| `--note TEXT` | – | free-text note saved to `meta.json` |

### The coverage matrix to capture
Capture each cell (closes the decode false-positive risk — see
`../docs/CAPTURE_FIXTURE_FORMAT.md`):

`{16bit, 24bit} × {rest, strong_contraction, nerve_ulnar, nerve_median, nerve_radial}`

Ready-to-run (24-bit row; repeat with `--bits 16`):
```sh
for c in rest strong_contraction nerve_ulnar nerve_median nerve_radial; do
  python tools/capture_session.py --bits 24 --condition "$c" --hand right \
      --duration 30 --out "fixtures/sessions/24bit_$c"
  echo "captured $c — reposition / rest, then press Enter"; read
done
```

### Troubleshooting
- **"No Mudra Link found"** — power-cycle the band, bring it close, raise
  `--scan-timeout`. On macOS, grant Bluetooth permission to your terminal app
  (System Settings → Privacy & Security → Bluetooth).
- **macOS addresses** — CoreBluetooth hides the MAC and exposes a per-host UUID;
  prefer **name scanning** (the default). If you pass `--address`, use the UUID that
  the scan prints, not a MAC.
- **"WARNING: no SNC frames captured"** — the band wasn't streaming. Confirm it's
  worn and active; try a longer `--post-connect-delay`; re-run.
- **MTU < 247 (macOS)** — expected; capture anyway. The per-sample SNC layout is
  MTU-invariant; only samples-per-notification changes.
- **Tool crashes mid-run** — `capture.bin` is written incrementally so partial data
  survives, but `index.json`/`meta.json` are written on clean exit only. Paste the
  error and it'll be fixed.

---

## `inspect_capture.py` — sanity-check a capture
Stdlib only. Prints per-characteristic event counts and SNC byte-rate indicators
(notif/s, bytes/s, payload-size histogram) — the input to the raw-rate viability
question (`../docs/CLOCK_MODEL.md`).
```sh
python tools/inspect_capture.py fixtures/sessions/24bit_strong_contraction
```
