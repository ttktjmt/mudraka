# tools/ — host-side capture tooling

Python utilities for recording Mudra Link sessions into mudraka test fixtures.
**Not** part of the mudraka C++ core or its build; these run on a host with the
physical band. Fixture format: [`../docs/CAPTURE_FIXTURE_FORMAT.md`](../docs/CAPTURE_FIXTURE_FORMAT.md).

## Setup
```sh
python3 -m venv .venv && . .venv/bin/activate
pip install -r tools/requirements.txt
```

## Record a session (`capture_session.py`)
Captures **all characteristics, both directions** (emulates production; replayable
into the oracle). Run on the actual target OS (MTU/IMU routing are platform-specific).

```sh
# 24-bit, strong contraction, 60 s
python tools/capture_session.py --bits 24 --condition strong_contraction \
    --hand right --duration 60 --out fixtures/sessions/24bit_strong_contraction

# 16-bit, until Ctrl-C, known address (skip scan)
python tools/capture_session.py --bits 16 --condition rest \
    --address XX:XX:XX:XX:XX:XX --out fixtures/sessions/16bit_rest
```
Writes `capture.bin` / `index.json` / `meta.json` into `--out`.

### Coverage matrix to capture (see CAPTURE_FIXTURE_FORMAT.md)
`{16bit, 24bit} × {rest, strong_contraction, nerve_ulnar, nerve_median, nerve_radial}`

## Inspect a capture (`inspect_capture.py`)
Sanity-check and read off raw byte-rate indicators (stdlib only):
```sh
python tools/inspect_capture.py fixtures/sessions/24bit_strong_contraction
```
