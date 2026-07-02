# mudraka

[![npm](https://img.shields.io/npm/v/mudraka?logo=npm)](https://www.npmjs.com/package/mudraka)

WASM build of **mudraka** — a C++17 engine that decodes the Mudra Link wristband's raw
surface-EMG (SNC) BLE stream into `int32` samples, buffers them in a ring, and serves the
latest window for real-time use. Built for the browser (Web Bluetooth), Web Workers, and
Node. Ships `mudraka.js` + `mudraka.wasm` and TypeScript types.

> The engine is transport-agnostic: **you** own the BLE connection (Web Bluetooth) and
> hand each notification's bytes to `feed()`. mudraka does the decode + buffering.

## Install

```sh
npm install mudraka
```

## Quick start

```js
import createMudraka from "mudraka";

const M = await createMudraka();          // instantiate the WASM module
const CH = 3, MAX = 256;                   // 3 channels; max samples per pull
const stream = new M.Stream(M.makeConfig(CH, 834, 4)); // 3ch, ~834 Hz, 4 s ring

// A destination region in the WASM heap: channel-major (CH * MAX) int32.
const ptr = M._malloc(CH * MAX * 4);
const base = ptr >> 2;                      // int32 index into HEAP32
let cursor = 0;

// Per BLE notification (bytes = Uint8Array), decode then drain the new samples:
function onNotification(bytes, recvTimeS) {
  stream.feed(bytes, recvTimeS);
  const r = stream.pullInto(cursor, ptr, MAX);
  for (let i = 0; i < r.written; i++) {
    const ulnar  = M.HEAP32[base + 0 * MAX + i];
    const median = M.HEAP32[base + 1 * MAX + i];
    const radial = M.HEAP32[base + 2 * MAX + i];
    // …plot / forward…
  }
  cursor = r.next_cursor;                   // r.lost > 0 means the ring overwrote unread samples
}

// When done:
M._free(ptr);
stream.delete();                            // free the C++ object (embind)
```

## Web Bluetooth wiring

```js
char.addEventListener("characteristicvaluechanged", (e) => {
  const bytes = new Uint8Array(e.target.value.buffer); // DataView -> Uint8Array
  onNotification(bytes, performance.now() / 1000);
});
await char.startNotifications();
```

The SNC characteristic is `0000fff4-0000-1000-8000-00805f9b34fb`.

## Loading the `.wasm` with a bundler

By default the module fetches `mudraka.wasm` from the same URL as `mudraka.js`. Bundlers
that hash/relocate assets may break that — point `locateFile` at the emitted asset:

```js
import wasmUrl from "mudraka/mudraka.wasm?url"; // Vite; webpack: use asset/resource
const M = await createMudraka({ locateFile: () => wasmUrl });
```

## API

`createMudraka(options?)` → `Promise<Module>`. `options.locateFile?(path, dir)` resolves the
`.wasm`. The module exposes:

- `makeConfig(channels, nominalRateHz, ringSeconds)` → `Config`
- `new Stream(config)` — decode stream (one per connection):
  - `feed(bytes: Uint8Array, recvTimeS: number)` → samples written per channel (0 if malformed)
  - `pullInto(cursor, dstPtr, max)` → `{ written, next_cursor, lost }` (writes channel-major int32 at `dstPtr`)
  - `head()` — total samples per channel so far (the newest cursor)
  - `channels()`, `estimatedRateHz()`, `malformedFrames()`, `totalOverwritten()`
  - `lastDeviceTimeUs()` — device-clock µs of the last notification (−1 if none)
  - `timestamp(i)` — reconstructed host time (s) for absolute sample index `i`
  - `delete()` — free the C++ object
- `_malloc(bytes)` / `_free(ptr)` and `HEAP32` for the `pullInto` destination.

Types are bundled (`mudraka.d.ts`).

## Links

Source, design docs, and the Python (PyPI) build: **https://github.com/ttktjmt/mudraka**.
Licensed Apache-2.0.
