#!/usr/bin/env python3
"""Record a full Mudra Link BLE session to a mudraka test fixture.

Captures *every* byte on *every* characteristic, in *both* directions
(device->host notifications and host->device command writes), so the recording
emulates the production environment and can be replayed in full into the official
MudraSDK oracle. See docs/CAPTURE_FIXTURE_FORMAT.md.

Output (one directory):
    <out>/capture.bin     exact wire bytes, all chars, both directions, in order
    <out>/index.json      per-event {i, offset, len, uuid, dir, t_mono_ns, t_wall}
    <out>/meta.json       capture conditions (incl. negotiated MTU)

Run on the ACTUAL target OS (e.g. macOS for a macOS mudra-lsl) to emulate
production; the negotiated MTU and IMU routing differ per platform.

Requires: bleak  (pip install bleak)

Examples:
    python tools/capture_session.py --bits 24 --condition strong_contraction \
        --hand right --duration 60 --out fixtures/sessions/24bit_strong_contraction
    python tools/capture_session.py --bits 16 --enable snc --enable imu_acc \
        --address XX:XX:XX:XX:XX:XX        # skip scan
"""
from __future__ import annotations

import argparse
import asyncio
import json
import platform
import signal
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
except ImportError:  # allow --help without bleak; fail clearly when actually run
    BleakClient = BleakScanner = None

TOOL_VERSION = "0.1"

# --- Mudra Link GATT (see docs/MUDRA_LINK_SIGNAL_SPEC.md) ---------------------
SNC = "0000fff4-0000-1000-8000-00805f9b34fb"
IMU = "0000fff5-0000-1000-8000-00805f9b34fb"
COMMAND = "0000fff1-0000-1000-8000-00805f9b34fb"
LOGGING = "0000fff2-0000-1000-8000-00805f9b34fb"
MESSAGE = "0000fff6-0000-1000-8000-00805f9b34fb"
BATTERY = "00002a19-0000-1000-8000-00805f9b34fb"
CHARGING = "00002a1a-0000-1000-8000-00805f9b34fb"
FW_VERSION = "00002a26-0000-1000-8000-00805f9b34fb"
SERIAL_RIGHT = "00002a25-0000-1000-8000-00805f9b34fb"
SERIAL_LEFT = "00002a27-0000-1000-8000-00805f9b34fb"

# Subscribe to everything that notifies -- we record all traffic, not just SNC.
NOTIFY_CHARS = [SNC, IMU, COMMAND, LOGGING, MESSAGE, BATTERY, CHARGING]

# Enable commands written to COMMAND (host->device). Mapping per Prodilink / spec.
ENABLE_CMDS = {
    "snc": bytes([0x06, 0x00, 0x01]),
    "pressure": bytes([0x06, 0x01, 0x01]),
    "imu_acc": bytes([0x07, 0x03, 0x01]),
    "imu_gyro": bytes([0x07, 0x02, 0x01]),
    "imu_quaternion": bytes([0x07, 0x01, 0x01]),
    "gestures": bytes([0x07, 0x08, 0x01]),
    "navigation": bytes([0x07, 0x07, 0x01]),
}
# SET_SAMPLE_TYPE: 22 00 = 16bit, 22 01 = 24bit (Prodilink device.py {"16bit":0,"24bit":1})
SAMPLE_TYPE_CMD = {16: bytes([0x22, 0x00]), 24: bytes([0x22, 0x01])}


class SessionRecorder:
    """Writes capture.bin incrementally and accumulates the index in memory."""

    def __init__(self, out_dir: Path):
        self.out_dir = out_dir
        self.bin_path = out_dir / "capture.bin"
        self._bin = open(self.bin_path, "wb")
        self._offset = 0
        self._t0 = time.monotonic_ns()
        self.frames: list[dict] = []
        self.by_uuid: dict[str, int] = {}

    def record(self, uuid: str, direction: str, data: bytes) -> None:
        b = bytes(data)
        self._bin.write(b)
        self.frames.append({
            "i": len(self.frames),
            "offset": self._offset,
            "len": len(b),
            "uuid": uuid,
            "dir": direction,
            "t_mono_ns": time.monotonic_ns() - self._t0,
            "t_wall": time.time(),
        })
        self._offset += len(b)
        self.by_uuid[uuid] = self.by_uuid.get(uuid, 0) + 1

    def close(self) -> None:
        self._bin.flush()
        self._bin.close()

    def write_index(self) -> None:
        (self.out_dir / "index.json").write_text(
            json.dumps({"frames": self.frames}, indent=0)
        )

    def write_meta(self, meta: dict) -> None:
        (self.out_dir / "meta.json").write_text(json.dumps(meta, indent=2))


async def _read_str(client: BleakClient, uuid: str) -> str | None:
    try:
        return bytes(await client.read_gatt_char(uuid)).decode("utf-8", "replace").strip()
    except Exception:
        return None


async def _read_hex(client: BleakClient, uuid: str) -> str | None:
    try:
        return bytes(await client.read_gatt_char(uuid)).hex()
    except Exception:
        return None


async def _write_cmd(client: BleakClient, rec: SessionRecorder, data: bytes) -> None:
    """Write a COMMAND and record it as a tx frame (try with/without response)."""
    rec.record(COMMAND, "tx", data)
    try:
        await client.write_gatt_char(COMMAND, data, response=True)
    except Exception:
        await client.write_gatt_char(COMMAND, data, response=False)


async def run(args: argparse.Namespace) -> int:
    if BleakClient is None:
        sys.exit("bleak is required: pip install bleak (see tools/requirements.txt)")
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    # --- find device ---
    address = args.address
    name = None
    if not address:
        print(f"Scanning for '{args.name_filter}' (timeout {args.scan_timeout}s)...")
        dev = await BleakScanner.find_device_by_filter(
            lambda d, ad: bool(d.name and args.name_filter.lower() in d.name.lower()),
            timeout=args.scan_timeout,
        )
        if not dev:
            print("No Mudra Link found. Power it on / bring it close, or pass --address.")
            return 1
        address, name = dev.address, dev.name
        print(f"Found {name} ({address})")

    rec = SessionRecorder(out_dir)
    stop = asyncio.Event()

    def _request_stop(*_):
        stop.set()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            asyncio.get_running_loop().add_signal_handler(sig, _request_stop)
        except (NotImplementedError, RuntimeError):
            pass

    print(f"Connecting to {address}...")
    async with BleakClient(address) as client:
        if args.post_connect_delay:
            await asyncio.sleep(args.post_connect_delay)

        mtu = getattr(client, "mtu_size", None)
        fw = await _read_str(client, FW_VERSION)
        serial = await _read_hex(client, SERIAL_RIGHT)
        print(f"Connected. MTU={mtu} fw={fw}")

        # subscribe to all notify chars
        def make_cb(uuid: str):
            return lambda sender, data: rec.record(uuid, "rx", data)

        subscribed = []
        for uuid in NOTIFY_CHARS:
            try:
                await client.start_notify(uuid, make_cb(uuid))
                subscribed.append(uuid)
            except Exception as e:
                print(f"  (subscribe failed for ...{uuid[4:8]}: {type(e).__name__})")
        print(f"Subscribed to {len(subscribed)}/{len(NOTIFY_CHARS)} characteristics.")

        # set sample type + enable requested streams (recorded as tx)
        await _write_cmd(client, rec, SAMPLE_TYPE_CMD[args.bits])
        for stream in args.enable:
            await _write_cmd(client, rec, ENABLE_CMDS[stream])
        print(f"sample_type={args.bits}bit  enabled={args.enable}")

        # record
        if args.duration:
            print(f"Recording for {args.duration}s (Ctrl-C to stop early)...")
            try:
                await asyncio.wait_for(stop.wait(), timeout=args.duration)
            except asyncio.TimeoutError:
                pass
        else:
            print("Recording until Ctrl-C...")
            await stop.wait()

        # disable SNC politely; stop notifications
        try:
            await _write_cmd(client, rec, bytes([0x06, 0x00, 0x00]))
        except Exception:
            pass
        for uuid in subscribed:
            try:
                await client.stop_notify(uuid)
            except Exception:
                pass

    rec.close()
    duration_s = (rec.frames[-1]["t_mono_ns"] / 1e9) if rec.frames else 0.0
    rec.write_index()
    rec.write_meta({
        "sample_type": f"{args.bits}bit",
        "enabled_streams": args.enable,
        "mtu": mtu,
        "fw_version": fw,
        "hand": args.hand,
        "serial": serial,
        "condition": args.condition,
        "note": args.note,
        "platform": platform.system().lower(),
        "duration_s": round(duration_s, 3),
        "n_frames": len(rec.frames),
        "frames_by_uuid": rec.by_uuid,
        "capture_tool": "tools/capture_session.py",
        "capture_tool_version": TOOL_VERSION,
        "captured_at": datetime.now(timezone.utc).isoformat(),
    })

    snc_n = rec.by_uuid.get(SNC, 0)
    print(f"\nDone. {len(rec.frames)} frames ({snc_n} SNC) over {duration_s:.1f}s "
          f"-> {out_dir}/")
    if snc_n == 0:
        print("WARNING: no SNC frames captured -- check the band was streaming.")
    return 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", required=True, help="output fixture dir, e.g. fixtures/sessions/24bit_rest")
    p.add_argument("--address", help="BLE address (skip scan)")
    p.add_argument("--name-filter", default="mudra", help="device name substring (default: mudra)")
    p.add_argument("--scan-timeout", type=float, default=10.0)
    p.add_argument("--post-connect-delay", type=float, default=2.0,
                   help="seconds to wait after connect before enabling streams")
    p.add_argument("--bits", type=int, choices=(16, 24), default=24, help="SNC sample width")
    p.add_argument("--enable", action="append", choices=sorted(ENABLE_CMDS), default=None,
                   help="streams to enable (repeatable; default: snc)")
    p.add_argument("--duration", type=float, default=None, help="seconds (default: until Ctrl-C)")
    p.add_argument("--hand", choices=("left", "right"), default=None)
    p.add_argument("--condition", default=None,
                   help="rest | strong_contraction | nerve_ulnar | nerve_median | nerve_radial | mixed")
    p.add_argument("--note", default=None)
    a = p.parse_args()
    if not a.enable:
        a.enable = ["snc"]
    return a


if __name__ == "__main__":
    try:
        raise SystemExit(asyncio.run(run(parse_args())))
    except KeyboardInterrupt:
        pass
