# mudraka

[![PyPI](https://img.shields.io/pypi/v/mudraka?logo=pypi&logoColor=white)](https://pypi.org/project/mudraka/)
[![npm](https://img.shields.io/npm/v/mudraka?logo=npm)](https://www.npmjs.com/package/mudraka)

A transport-agnostic **C++17 engine** that decodes the Mudra Link wristband's raw
surface-EMG (SNC) stream into samples, buffers them, and serves them for real-time
use. It is the shared core of three projects (this repo is **mudraka only**):

- **mudraka** — this engine.
- **mudra-lsl** — Python/PyPI wrapper publishing LSL (consumes the wheel).
- **mudra-web-viewer** — Web Bluetooth live viewer (consumes the WASM/npm build).

One C++ core fans out to two distributions: **npm** (Emscripten→WASM) and **PyPI**
(nanobind wheel). See **[docs/](docs/README.md)** for the full design (start with
[docs/CONTEXT.md](docs/CONTEXT.md)).

## Status

| Part | State |
|------|-------|
| Native C++ core (decoder, ring, clock, stream, diagnostics) | ✅ implemented, tested against real captures (22k+ assertions) |
| Python binding (nanobind) | ✅ builds & verified end-to-end on real fixtures |
| WASM binding (Embind) | ✅ written & CMake-wired (build needs the Emscripten toolchain) |
| **SNC decode** | ✅ decoded **directly**, empirically validated for Mudra Link (16-bit, ~834 Hz); no official lib/oracle (see [docs/DECODE_VERIFICATION.md](docs/DECODE_VERIFICATION.md)) |

> Target is the **retail Mudra Link** (~834 Hz / 16-bit — the vendor-confirmed limit).
> The 2080 Hz figure is a **separate product, Mudra Pro**, a future target that slots in
> as a new `IDecoder` — see [docs/CONTEXT.md](docs/CONTEXT.md).

## Layout

```
include/mudraka/   public headers          src/        core implementation
tests/             doctest suite           bindings/   python (nanobind) + wasm (embind)
tools/             BLE capture (bleak)      fixtures/   recorded test sessions
docs/              design docs (indexed by docs/README.md)
```

## Build & test (native)

```sh
cmake -S . -B build -G Ninja -DMUDRAKA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure      # or: ./build/tests/mudraka_tests
```

## Python wheel

```sh
pip install .            # scikit-build-core + nanobind; needs a C++17 compiler
```
```python
import numpy as np
from mudraka import Stream, Config
s = Stream(Config())
s.feed(notification_bytes, recv_time_s)          # one BLE notification per call
out = np.empty((3, 4096), dtype=np.int32)
written, cursor, lost = s.latest_into(out)       # zero-copy into `out`
```

## WASM (npm)

```sh
emcmake cmake -S . -B build-wasm -DMUDRAKA_BUILD_WASM=ON -DMUDRAKA_BUILD_TESTS=OFF
cmake --build build-wasm                         # -> mudraka.js + mudraka.wasm
```

## Recording fixtures

See [tools/README.md](tools/README.md) — `capture_session.py` records a full BLE
session from a real band.
