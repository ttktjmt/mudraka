#!/usr/bin/env python3
"""Sanity-check a captured session and report raw-layer rate indicators.

Reads a fixture dir produced by capture_session.py and prints per-characteristic
event counts plus SNC framing stats. Decoding the SNC layout is still unknown, so
this reports *byte-rate* indicators (notifications/s, bytes/s, payload-size
histogram) -- enough to sanity-check a capture and to feed the raw-rate viability
question once bytes-per-sample is known (see docs/CLOCK_MODEL.md).

Stdlib only.

    python tools/inspect_capture.py fixtures/sessions/24bit_rest
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

SNC = "0000fff4-0000-1000-8000-00805f9b34fb"
SHORT = {  # pretty names for known UUIDs
    "0000fff4-0000-1000-8000-00805f9b34fb": "SNC",
    "0000fff5-0000-1000-8000-00805f9b34fb": "IMU",
    "0000fff1-0000-1000-8000-00805f9b34fb": "COMMAND",
    "0000fff2-0000-1000-8000-00805f9b34fb": "LOGGING",
    "0000fff6-0000-1000-8000-00805f9b34fb": "MESSAGE",
    "00002a19-0000-1000-8000-00805f9b34fb": "BATTERY",
    "00002a1a-0000-1000-8000-00805f9b34fb": "CHARGING",
}


def main(d: Path) -> int:
    idx = json.loads((d / "index.json").read_text())["frames"]
    meta = json.loads((d / "meta.json").read_text()) if (d / "meta.json").exists() else {}
    if not idx:
        print("empty capture")
        return 1

    print(f"== {d} ==")
    if meta:
        print(f"sample_type={meta.get('sample_type')} mtu={meta.get('mtu')} "
              f"platform={meta.get('platform')} condition={meta.get('condition')} "
              f"duration={meta.get('duration_s')}s")

    # per-(uuid,dir) counts
    counts = Counter((f["uuid"], f["dir"]) for f in idx)
    print("\nevents by characteristic / direction:")
    for (uuid, dr), n in sorted(counts.items(), key=lambda x: -x[1]):
        print(f"  {SHORT.get(uuid, uuid[:8]):8s} {dr:2s}  {n}")

    # SNC framing + byte-rate indicators
    snc = [f for f in idx if f["uuid"] == SNC and f["dir"] == "rx"]
    if not snc:
        print("\nNO SNC rx frames -- band was not streaming SNC.")
        return 1
    span_ns = snc[-1]["t_mono_ns"] - snc[0]["t_mono_ns"]
    span_s = span_ns / 1e9 if span_ns else 0.0
    total_bytes = sum(f["len"] for f in snc)
    sizes = Counter(f["len"] for f in snc)
    print(f"\nSNC: {len(snc)} notifications, {total_bytes} bytes over {span_s:.2f}s")
    if span_s:
        print(f"     {len(snc)/span_s:.1f} notif/s, {total_bytes/span_s:.0f} bytes/s")
    print("     payload sizes (bytes: count): " +
          ", ".join(f"{sz}:{n}" for sz, n in sorted(sizes.items())))
    print("\nNote: sample rate = bytes/s / (bytes_per_sample * channels); fill in once")
    print("      the decode layout is known. See docs/CLOCK_MODEL.md (viability).")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: python tools/inspect_capture.py <fixture-dir>")
    raise SystemExit(main(Path(sys.argv[1])))
