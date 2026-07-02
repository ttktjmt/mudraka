# mudraka

[![PyPI](https://img.shields.io/pypi/v/mudraka?logo=pypi&logoColor=white)](https://pypi.org/project/mudraka/)
[![npm](https://img.shields.io/npm/v/mudraka?logo=npm)](https://www.npmjs.com/package/mudraka)

A transport-agnostic **C++17 engine** that decodes the Mudra Link wristband's raw
surface-EMG (SNC) BLE stream into `int32` samples, buffers them in a ring, and serves
the latest window for real-time use. **You** own the BLE connection and hand each
notification's bytes to `feed()`; mudraka does the decode + buffering.

One core, two distributions: **PyPI** (nanobind wheel) and **npm** (Emscripten→WASM,
with TypeScript types). Target is the retail **Mudra Link** (~834 Hz / 16-bit).

## Python

```sh
pip install mudraka
```
```python
import numpy as np
from mudraka import Stream, Config

s = Stream(Config())
s.feed(notification_bytes, recv_time_s)          # one BLE notification per call
out = np.empty((3, 4096), dtype=np.int32)
written, cursor, lost = s.latest_into(out)       # zero-copy into `out`; lost > 0 = ring overwrote unread
```

## JavaScript / TypeScript

```sh
npm install mudraka
```
```js
import createMudraka from "mudraka";

const M = await createMudraka();
const CH = 3, MAX = 256;
const stream = new M.Stream(M.makeConfig(CH, 834, 4)); // 3ch, ~834 Hz, 4 s ring

const ptr = M._malloc(CH * MAX * 4), base = ptr >> 2;  // channel-major int32 in WASM heap
let cursor = 0;

function onNotification(bytes, recvTimeS) {            // bytes = Uint8Array
  stream.feed(bytes, recvTimeS);
  const r = stream.pullInto(cursor, ptr, MAX);
  for (let i = 0; i < r.written; i++) {
    const ulnar = M.HEAP32[base + 0 * MAX + i];        // + median, radial at 1*, 2*
  }
  cursor = r.next_cursor;                              // r.lost > 0 = ring overwrote unread
}
// M._free(ptr); stream.delete();  when done
```

Web Bluetooth wiring, the bundler `.wasm` note, and the full JS API are in
[npm/README.md](npm/README.md).

## Build from source (native)

```sh
cmake -S . -B build -G Ninja -DMUDRAKA_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Design docs are in **[docs/](docs/README.md)** (start with
[docs/CONTEXT.md](docs/CONTEXT.md)). SNC decode details:
[docs/DECODE_VERIFICATION.md](docs/DECODE_VERIFICATION.md).

## Acknowledgements

- [Prodilink](https://github.com/JayTheProdigy16/Prodilink) : reverse-engineering
  reference for the Mudra Link BLE protocol.
