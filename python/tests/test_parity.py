"""Golden-driven decode parity for the Python wheel (docs/TEST_STRATEGY.md).

Each fixture's committed expected.jsonl is the native reference decode; the wheel
must reproduce it bit-exact. One JSON object per SNC notification:
{"snc_ts": <device_time_us>, "samples": [[u,m,r], ...]}. Regenerate goldens from
native (MUDRAKA_REGEN_GOLDEN=1), never here.
"""
import json
import pathlib

import numpy as np
import pytest

from mudraka import Config, Stream

ROOT = pathlib.Path(__file__).resolve().parents[2]
SNC = "0000fff4-0000-1000-8000-00805f9b34fb"
FIXTURES = ["16bit_rest", "24bit_rest", "24bit_strong_contraction"]


def _load(name):
    d = ROOT / "fixtures" / "sessions" / name
    blob = (d / "capture.bin").read_bytes()
    frames = [
        f
        for f in json.loads((d / "index.json").read_text())["frames"]
        if f["dir"] == "rx" and f["uuid"] == SNC
    ]
    golden = [json.loads(ln) for ln in (d / "expected.jsonl").read_text().splitlines() if ln]
    return blob, frames, golden


@pytest.mark.parametrize("name", FIXTURES)
def test_decode_matches_golden(name):
    blob, frames, golden = _load(name)
    assert len(frames) == len(golden), "frame/golden count mismatch"

    s = Stream(Config())  # 3ch, ~834 Hz, 4 s ring
    out = np.empty((3, 64), dtype=np.int32)  # >= samples per notification
    cursor = 0
    for f, g in zip(frames, golden):
        payload = bytes(blob[f["offset"] : f["offset"] + f["len"]])
        n = s.feed(payload, f["t_mono_ns"] / 1e9)
        assert n == len(g["samples"])
        assert s.stats().last_device_time_us == g["snc_ts"]
        written, cursor, lost = s.pull_into(cursor, out)
        assert lost == 0 and written == n
        assert out[:, :written].T.tolist() == g["samples"]
