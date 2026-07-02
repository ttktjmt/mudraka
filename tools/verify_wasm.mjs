// Golden-driven WASM decode parity (docs/TEST_STRATEGY.md) + end-to-end smoke, run
// under Node. Feeds a real captured session through the embind Stream one SNC
// notification at a time and asserts, bit-exact, that snc_ts + samples match the
// committed expected.jsonl (the native reference decode). Run:
//   node tools/verify_wasm.mjs [fixtures/sessions/<name>]
// Exits non-zero on any mismatch.
import { readFileSync } from "node:fs";
import { strict as assert } from "node:assert";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..");
const dir = resolve(root, process.argv[2] ?? "fixtures/sessions/24bit_strong_contraction");
const SNC = "0000fff4-0000-1000-8000-00805f9b34fb";

const blob = readFileSync(`${dir}/capture.bin`);
const frames = JSON.parse(readFileSync(`${dir}/index.json`, "utf8")).frames.filter(
  (f) => f.dir === "rx" && f.uuid === SNC,
);
const golden = readFileSync(`${dir}/expected.jsonl`, "utf8")
  .split("\n")
  .filter((l) => l)
  .map((l) => JSON.parse(l));
assert(frames.length > 0, "no SNC frames in fixture");
assert.equal(frames.length, golden.length, "frame/golden count mismatch");

const createMudraka = (await import(resolve(root, "build-wasm/bindings/wasm/mudraka.js"))).default;
const M = await createMudraka();

const CH = 3, MAX = 64; // >= samples per notification
const stream = new M.Stream(M.makeConfig(CH, 834.0, 4)); // 3ch, ~834 Hz, 4 s ring
const ptr = M._malloc(CH * MAX * 4);
const base = ptr >> 2; // int32 index into HEAP32

let cursor = 0;
let peak = 0;
for (let k = 0; k < frames.length; k++) {
  const f = frames[k];
  const g = golden[k];
  const payload = new Uint8Array(blob.buffer, blob.byteOffset + f.offset, f.len);
  const n = stream.feed(payload, f.t_mono_ns / 1e9);
  assert.equal(n, g.samples.length, `notification ${k}: sample count`);
  assert.equal(stream.lastDeviceTimeUs(), g.snc_ts, `notification ${k}: snc_ts`);

  const r = stream.pullInto(cursor, ptr, MAX);
  assert.equal(r.lost, 0, `notification ${k}: ring overflow`);
  assert.equal(r.written, n, `notification ${k}: drained count`);
  cursor = r.next_cursor;
  for (let i = 0; i < r.written; i++) {
    const s = [M.HEAP32[base + i], M.HEAP32[base + MAX + i], M.HEAP32[base + 2 * MAX + i]];
    assert.deepEqual(s, g.samples[i], `notification ${k}, sample ${i}`);
    peak = Math.max(peak, Math.abs(s[0]), Math.abs(s[1]), Math.abs(s[2]));
  }
}
M._free(ptr);

const expected = frames.length * 18;
assert.equal(stream.head(), expected, "head != frames*18");
assert.equal(stream.malformedFrames(), 0, "malformed frames present");
console.log(
  `dir=${dir.split("/").pop()} notifications=${frames.length} samples=${stream.head()} ` +
    `rate=${stream.estimatedRateHz().toFixed(1)}Hz peak=${peak} — golden parity OK`,
);
