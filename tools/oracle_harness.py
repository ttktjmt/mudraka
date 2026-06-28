#!/usr/bin/env python3
"""Generate the golden `expected.jsonl` for a session via the official MudraSDK lib.

This is the (β) differential oracle of docs/DECODE_VERIFICATION.md: replay a session's
captured SNC notifications through the real `MudraSDK` compute library with recording
enabled, and dump the layer-A `SNC_NO_FACTOR1..3` (+ `SNC_TS`) it produces. mudraka's
int32 decode must then match this bit-exact.

Status: READY TO RUN once the desktop MudraSDK shared lib is available (Track B). Two
things must be supplied by the user (marked TODO below) because they live in the
closed lib / SDK and cannot be inferred here:
  1. --lib : path to the desktop MudraSDK shared library (.dylib/.so/.dll).
  2. the RecordingDataType integer ids for SNC_NO_FACTOR1..3 and SNC_TS
     (from re/.../enums/RecordingDataType.java in the SDK).

ctypes signatures mirror re/python_sdk/.../computation_wrapper.py.

    python tools/oracle_harness.py --lib /path/to/MudraSDK.dylib \
        --session fixtures/sessions/24bit_rest
"""
from __future__ import annotations

import argparse
import ctypes as C
import json
from pathlib import Path

SNC_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"

# TODO(Track B): fill these from the SDK's RecordingDataType.java. Names are known
# (docs/MUDRA_LINK_SIGNAL_SPEC.md §4.4); the integer ids are not — confirm before use.
RECORDING_TYPE_IDS = {
    "SNC_NO_FACTOR1": None,
    "SNC_NO_FACTOR2": None,
    "SNC_NO_FACTOR3": None,
    "SNC_TS": None,
}


def bind(lib_path: str) -> C.CDLL:
    lib = C.CDLL(lib_path)
    Handle = C.c_void_p
    lib.create_computation.argtypes = [C.c_int, C.c_char_p, C.c_char_p]
    lib.create_computation.restype = Handle
    lib.handle_data.argtypes = [Handle, C.POINTER(C.c_uint8), C.c_size_t]
    lib.handle_data.restype = None
    lib.enable_recording.argtypes = [Handle]
    lib.enable_recording.restype = None
    lib.start_recording.argtypes = [Handle, C.c_char_p, C.POINTER(C.c_int), C.c_int, C.c_int]
    lib.start_recording.restype = C.c_bool
    lib.stop_recording.argtypes = [Handle]
    lib.stop_recording.restype = None
    lib.get_json_recording.argtypes = [Handle]
    lib.get_json_recording.restype = C.c_char_p
    return lib


def load_snc_frames(session: Path) -> list[bytes]:
    blob = (session / "capture.bin").read_bytes()
    idx = json.loads((session / "index.json").read_text())["frames"]
    return [blob[f["offset"]:f["offset"] + f["len"]]
            for f in idx if f["dir"] == "rx" and f["uuid"] == SNC_UUID]


def main(args: argparse.Namespace) -> int:
    ids = [RECORDING_TYPE_IDS[k] for k in ("SNC_NO_FACTOR1", "SNC_NO_FACTOR2",
                                           "SNC_NO_FACTOR3", "SNC_TS")]
    if any(v is None for v in ids):
        raise SystemExit(
            "Fill RECORDING_TYPE_IDS (SNC_NO_FACTOR1..3, SNC_TS) from the SDK's "
            "RecordingDataType.java before running — see the module docstring.")

    session = Path(args.session)
    frames = load_snc_frames(session)
    if not frames:
        raise SystemExit(f"no SNC frames in {session}")

    lib = bind(args.lib)
    h = lib.create_computation(0, args.model_path.encode(), args.writable_path.encode())
    if not h:
        raise SystemExit("create_computation failed (license? model_path?)")

    lib.enable_recording(h)
    arr = (C.c_int * len(ids))(*ids)
    ok = lib.start_recording(h, b"{}", arr, len(ids), 0)
    if not ok:
        raise SystemExit("start_recording returned false (license tier / RawData?)")

    for fr in frames:                          # replay in capture order
        buf = (C.c_uint8 * len(fr)).from_buffer_copy(fr)
        lib.handle_data(h, buf, len(fr))

    lib.stop_recording(h)
    rec = json.loads(lib.get_json_recording(h).decode("utf-8"))

    out = session / "expected.jsonl"
    n = write_expected(rec, out)
    print(f"wrote {n} samples -> {out}")
    return 0


def write_expected(recording: dict, out: Path) -> int:
    """Map the SDK recording JSON -> expected.jsonl. The exact JSON shape depends on the
    SDK build, so this is left as a small adapter to finalize against a real dump."""
    # TODO(Track B): adapt to the actual get_json_recording() schema. Expected output:
    #   {"snc_ts": <int>, "no_factor": [u, m, r]} per line.
    raise SystemExit("Finalize write_expected() against a real get_json_recording() dump "
                     "(see docstring). The raw recording was obtained successfully.")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--lib", required=True, help="path to MudraSDK .dylib/.so/.dll")
    p.add_argument("--session", required=True, help="fixtures/sessions/<name> dir")
    p.add_argument("--model-path", default="./")
    p.add_argument("--writable-path", default="./")
    raise SystemExit(main(p.parse_args()))
