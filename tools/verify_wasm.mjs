// End-to-end WASM smoke check: feed a real captured session through the embind
// Stream and pull it back — mirrors tests/test_stream.cpp. Run under Node:
//   node tools/verify_wasm.mjs [fixtures/sessions/<name>]
// Exits non-zero on any failed assertion.
import { readFileSync } from "node:fs";
import { strict as assert } from "node:assert";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..");
const dir = resolve(root, process.argv[2] ?? "fixtures/sessions/24bit_strong_contraction");
const SNC = "0000fff4-0000-1000-8000-00805f9b34fb";

const blob = readFileSync(`${dir}/capture.bin`);
const index = JSON.parse(readFileSync(`${dir}/index.json`, "utf8"));
const frames = index.frames.filter((f) => f.dir === "rx" && f.uuid === SNC);
assert(frames.length > 0, "no SNC frames in fixture");

const createMudraka = (await import(resolve(root, "build-wasm/bindings/wasm/mudraka.js"))).default;
const M = await createMudraka();

const CH = 3, MAX = 4096;
const cfg = M.makeConfig(CH, 834.0, 4); // 3ch, ~834 Hz, 4 s ring
const stream = new M.Stream(cfg);

const ptr = M._malloc(CH * MAX * 4);
const base = ptr >> 2; // int32 index into HEAP32
const samples = []; // [[ulnar, median, radial], ...]

// Drain everything new since `cursor`, keeping up so the ring never overflows.
let cursor = 0;
const drain = () => {
  let r;
  do {
    r = stream.pullInto(cursor, ptr, MAX);
    for (let i = 0; i < r.written; i++)
      samples.push([M.HEAP32[base + i], M.HEAP32[base + MAX + i], M.HEAP32[base + 2 * MAX + i]]);
    cursor = r.next_cursor;
    assert.equal(r.lost, 0, "ring overflow: consumer fell behind");
  } while (r.written === MAX);
};

for (const f of frames) {
  const payload = new Uint8Array(blob.buffer, blob.byteOffset + f.offset, f.len);
  stream.feed(payload, f.t_mono_ns / 1e9);
  drain();
}
M._free(ptr);

const expected = frames.length * 18; // 18 samples per SNC notification
const peak = samples.reduce((m, s) => Math.max(m, Math.abs(s[0]), Math.abs(s[1]), Math.abs(s[2])), 0);
console.log(
  `dir=${dir.split("/").pop()} frames=${frames.length} head=${stream.head()} ` +
    `samples=${samples.length} malformed=${stream.malformedFrames()} ` +
    `rate=${stream.estimatedRateHz().toFixed(1)}Hz peak=${peak}`,
);
console.log("first sample:", samples[0], "last:", samples.at(-1));

assert.equal(stream.head(), expected, "head != frames*18");
assert.equal(samples.length, expected, "drained sample count mismatch");
assert.equal(stream.malformedFrames(), 0, "malformed frames present");
assert(peak > 0, "decoded signal is all-zero (decode likely broken)");
console.log("OK");
