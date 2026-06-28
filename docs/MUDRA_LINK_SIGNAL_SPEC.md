# Mudra Link — Signal Characteristics Spec Sheet (LLM context)

> **Purpose**: A technical reference describing every signal the Mudra Link wristband emits, written so an LLM can understand it without ambiguity.
> **Audience**: Developers / coding agents building Mudra Link tooling.
> **Generated**: 2026-06-24 · **Target**: Mudra Link (Wearable Devices Ltd, FCC ID `2A5CU-MUDRABAND`)

## Source legend

Every fact carries a source tag.

- **[OFFICIAL]** — Wearable Devices official site / FCC certification (external, primary source)
- **[RE]** — Reverse-engineering artifacts in this repository (decompiled SDKs / disassembled firmware under `re/`, plus the `prodilink/` implementation)

---

## ⚠️ Most important: "raw bytes" and "computed signal" are different layers

This is the easiest thing to confuse, so it comes first. **Two distinct layers** exist.

| Layer | What you get | Format | Where Prodilink sits |
|-------|--------------|--------|----------------------|
| **(A) Raw-byte layer** (BLE GATT) | The raw notification bytes the device sends | `int16` / `int32` count values, variable length | **This is the layer Prodilink receives directly.** `_dispatch()` / `on_raw_*` |
| **(B) Computed-signal layer** (official SDK native lib) | Values after normalization, scaling, RMS, and frequency computation | `float` (normalized −1..+1, etc.) + metadata | Not implemented in Prodilink (lives inside the official `libMudra*.so`) |

**Unless noted otherwise, the examples in this document are layer (A), raw bytes.** Quantities like the `−1..+1` range, RMS, and frequency are outputs of layer (B); you will not get them by reading the raw bytes directly.

---

## 1. Device overview

| Item | Value | Source |
|------|-------|--------|
| Product name | Mudra Link | [OFFICIAL] |
| Manufacturer | Wearable Devices Ltd. | [OFFICIAL] |
| BLE device name | Contains `"mudra"` (case-insensitive) | [RE] |
| Sensor suite | 3× SNC neural sensors + 6-DoF IMU | [OFFICIAL][RE] |
| Primary signal | Surface EMG (sEMG); vendor term SNC = Surface Nerve Conductance | [OFFICIAL] |
| Radio | Bluetooth Low Energy, 2402.0–2480.0 MHz (2.4 GHz ISM) | [OFFICIAL] |
| FCC ID | `2A5CU-MUDRABAND` (granted 2022-03-25) | [OFFICIAL] |

---

## 2. Hardware, power, radio

| Item | Value | Source |
|------|-------|--------|
| Weight | 36 g | [OFFICIAL] |
| Dimensions | 22 mm wide / 10 mm thick (incl. strap) | [OFFICIAL] |
| Ingress protection | IP56 | [OFFICIAL] |
| Materials | Biocompatible silicone / nylon fabric strap / stainless-steel electrodes | [OFFICIAL] |
| Battery | Up to 2 days / full charge in 80 min | [OFFICIAL] |
| Supported OS | Windows 10/11+, macOS 10.15+, ChromeOS, iPadOS 14+, Android 8.0+ | [OFFICIAL] |
| Reference price | $249–$299 | [OFFICIAL] |
| Requested MTU | 247 | [RE] |
| Firmware update | Nordic DFU (service `0xfe59`) | [RE] |

---

## 3. Signal channels (device → host)

All are BLE GATT notifications. Only COMMAND is bidirectional (also accepts host writes).

| UUID | Name | Direction/Type | Contents |
|------|------|----------------|----------|
| `0xfff4` | SNC | Notify | **Raw sEMG waveform** (3 nerves). Highest rate, highest volume |
| `0xfff5` | IMU | Notify | Raw 6-axis IMU data |
| `0xfff1` | COMMAND | R/W/Notify | Multiplexed channel (gesture, pressure, navigation, status… selected by byte[0]) |
| `0xfff6` | MESSAGE | Notify | Detailed battery, calibration-finished, secondary-device events |
| `0xfff2` | LOGGING | Notify | Firmware debug log (Prodilink does not parse it) |
| `0x2a19` | BATTERY | R/Notify | Battery level 0–100% |
| `0x2a1a` | CHARGING | R/Notify | Charging state |
| `0x2a26` | FIRMWARE_VERSION | Read | Firmware version (UTF-8 string) |
| `0x2a25/0x2a27` | SERIAL_RIGHT/LEFT | Read | Serial number (hex string) |

### Data flow

```
[Mudra Link device]
  3 sEMG electrodes + 6-axis IMU + on-device inference engine
        │  BLE notifications (link-layer encryption decrypted transparently by the OS)
        ▼
[GATT 7 characteristics]  fff4 / fff5 / fff1 / fff6 / fff2 / 2a19 / 2a1a
        │  decrypted plaintext bytes
        ▼
[BleConnection._dispatch(char_uuid, bytes)]   ← the single chokepoint every channel passes through
        │  parse_command_notification / parse_imu_packet / ...
        ▼
[MudraLink handlers] → [user callbacks]
  on_gesture / on_pressure / on_nav_delta / on_imu / on_raw_snc / on_raw_imu / ...
```

> There is no application-layer encryption (`encrypt/decrypt/cipher` returns 0 hits in the source [RE]). All data arrives as plaintext.

---

## 4. sEMG / SNC (primary signal) — `0xfff4`

The primary signal. Three surface electrodes at the wrist capture the electrical activity of the three peripheral nerves that drive the fingers.

### 4.1 Signal parameters

| Item | Value | Source |
|------|-------|--------|
| Channels | **3** (ulnar / median / radial nerves, one electrode each) | [OFFICIAL][RE] |
| Array mapping | `data[0]=ulnar, data[1]=median, data[2]=radial` (size 3) | [RE] |
| Sampling rate | **~1000 Hz** (nominal; companion-SDK protocol definition) | [RE] |
| Sample resolution | 16-bit / 24-bit selectable (command `SET_SAMPLE_TYPE 0x22`) | [RE] |
| Computed value range (layer B) | float, normalized **−1.0 .. +1.0** | [RE] |
| Accompanying metrics (layer B) | `timestamp`, `frequency`, `frequency_std`, `rms[]` | [RE] |
| Signal processing pipeline | electrodes → amplification → filtering (noise removal) → ADC → neural network | [OFFICIAL] |

### 4.2 Raw-byte example (layer A)

SNC raw data is a **variable-length stream of multi-byte samples**. Prodilink does not interpret it; it passes the bytes straight to `on_raw_snc(bytes)` (the exact byte width and channel-multiplexing layout are not decoded).

```
characteristic: 0xfff4
received bytes (hex):  a3 01 5f ff 12 02 e8 fe ...   (continues, variable length)
                       └─ sample stream. 16/24-bit, 3-channel multiplexing. Prodilink forwards it raw.
```

```python
# Receiving in Prodilink (layer A, raw bytes)
@device.on_raw_snc
def on_emg(data: bytes):
    print(len(data), data.hex(" "))   # e.g. 40 'a3 01 5f ff 12 02 ...'
```

### 4.3 Computed-signal example (layer B, official SDK)

The shape the official SDK's compute layer emits (from the companion-SDK protocol definition). Not obtainable from Prodilink alone.

```json
{
  "type": "snc",
  "data": { "values": [0.1, -0.05, 0.2], "frequency": 1000, "timestamp": 1234567890 }
}
```
- `values[0]=ulnar, values[1]=median, values[2]=radial`, each in −1.0 .. +1.0
- `frequency`: effective sample rate (≈1000 Hz)

### 4.4 Internal recording data types [RE]

The 32 channels defined by the official SDK's data recorder (`RecordingDataType.java`). The internal stages of the EMG pipeline are exposed here.

| Group | Channels | Meaning |
|-------|----------|---------|
| SNC (scaled) | `SNC1` `SNC2` `SNC3` | Scaled 3-nerve EMG |
| SNC (raw) | `SNC_NO_FACTOR1..3` | Raw EMG before factor is applied (the "rawest" values) |
| Neural outputs | `NEURAL_OUTPUT_SNC_A` `_B` | Neural-net outputs inferred from EMG |
| Time/aux | `SNC_TS` `SNC_APP_TS` `SNC_BUTTON` | Device time / app time / button sync |

---

## 5. IMU (6-axis motion) — `0xfff5`

3-axis accelerometer + 3-axis gyroscope.

### 5.1 Parameters

| Item | Value | Source |
|------|-------|--------|
| Configuration | 6-DoF (3 accel + 3 gyro) | [OFFICIAL] |
| Sampling | ~100 Hz (nominal) | [RE] |
| Accel unit (layer B) | m/s² (Z≈9.81 at rest) | [RE] |
| Gyro unit (layer B) | deg/s | [RE] |
| Derived streams | AccNorm / AccRaw / Gyro / Quaternion | [RE] |

### 5.2 Raw-byte example (layer A)

**1 sample = 12 bytes = `int16 LE × 6`** (ax, ay, az, gx, gy, gz). A single notification may concatenate multiple samples.

```
characteristic: 0xfff5
received bytes (hex):  e8 03  00 fe  40 1f  00 00  00 00  00 00
                       └ax──┘ └ay──┘ └az──┘ └gx──┘ └gy──┘ └gz──┘

decode (int16 LE, two's complement):
  ax = 0x03E8 =  1000
  ay = 0xFE00 =  -512
  az = 0x1F40 =  8000
  gx = gy = gz = 0
```

Concatenated-samples example (24 bytes = 2 samples):
```
e8 03 00 fe 40 1f 00 00 00 00 00 00 | f0 03 10 fe 38 1f 02 00 01 00 00 00
└────────── sample #1 ───────────┘ └────────── sample #2 ───────────┘
```

```python
# Receiving in Prodilink
import struct
@device.on_raw_imu
def on_imu(data: bytes):
    for off in range(0, len(data) - 11, 12):
        ax, ay, az, gx, gy, gz = struct.unpack_from("<6h", data, off)
        # Note: these are raw count values. Conversion to physical units is layer B's job.
```

> **Note**: The raw `int16` values are the device's internal ADC counts. The scale factors to convert to `m/s²` or `deg/s` live in the official SDK's compute layer (layer B). Prodilink's `parse_imu_packet` only does `float(value)` — it does not scale.

### 5.3 macOS path (IMU arrives via COMMAND)

On hosts that cannot negotiate Android-style connection parameters (MTU=247, etc.) — such as macOS — IMU arrives not on `0xfff5` but on **COMMAND (`0xfff1`) tag `0x70`**, and the values are **`int32 LE × 6`** (after a 5-byte header).

```
characteristic: 0xfff1
received bytes (hex):  70 XX XX XX XX | e8 03 00 00  00 fe ff ff  40 1f 00 00  ...
                       └tag┘└5B header┘ └─ax int32─┘ └─ay int32─┘ └─az int32─┘ ...
decode (int32 LE ×6 @ offset 5):
  ax=1000, ay=-512, az=8000, gx, gy, gz
```

---

## 6. COMMAND channel (multiplexed events) — `0xfff1`

The richest channel. **The first byte `byte[0]` is the event-type tag.**

### 6.1 Tag table

| tag | Type | Payload |
|-----|------|---------|
| `0x01` | general_status | 26+ bytes. Per-feature enable flags |
| `0x02` | airmouse_status | 18+ bytes. HID config |
| `0x03` (len=122) | mapper_data | HID mapper config blob |
| `0x06`/`0x07` | feature_state | Feature enable/disable echo |
| `0x0a` | stop_advertising | Advertising stopped |
| `0x22` | sample_type_changed | 16/24-bit switch confirmed |
| `0x59` | button | `byte[1]=0x01` → release |
| `0x60` | pressure | `byte[1]` = pressure 0–100 |
| `0x61` | gesture | `byte[1]` = gesture ID |
| `0x63` | firmware_crash | — |
| `0x64` | navigation_delta | dx, dy (signed16 LE ×2) |
| `0x65` | navigation_direction | `byte[1]` = direction enum |
| `0x70` (len≥29) | imu_data | IMU (int32×6, after 5B header) = macOS path |
| `0xbb` | band_connected | `byte[1]=0x01` |
| `0x93` | fuel_gauge_reset | — |
| `0x94` | battery_info_ack | Battery-request ACK (actual data on MESSAGE) |

### 6.2 Examples

```
gesture:               61 04          → byte[1]=0x04 = LEFT_PINCH
pinch pressure:        60 32          → byte[1]=0x32=50 → 50% → 0.50
navigation delta:      64 0a 00 fb ff → dx=signed16LE(0a,00)=+10, dy=signed16LE(fb,ff)=-5
navigation direction:  65 01          → byte[1]=1 = RIGHT
button release:        59 01          → release
band connected:        bb 01          → connected
```

Decoding signed16 LE:
```python
def signed16(low, high):
    v = (high << 8) | low
    return v - 0x10000 if v & 0x8000 else v
# 0x64 0a 00 fb ff → dx=signed16(0x0a,0x00)=10, dy=signed16(0xfb,0xff)=-5
```

### 6.3 Gesture IDs (byte[1] of tag `0x61`)

| ID | Gesture | ID | Gesture |
|----|---------|----|---------|
| `0x00` | TAP | `0x0A` | REVERSE_UP_PINCH |
| `0x01` | DOUBLE_TAP | `0x0B` | REVERSE_DOWN_PINCH |
| `0x02` | TWIST | `0x0C` | REVERSE_PINCH |
| `0x03` | DOUBLE_TWIST | `0x0D` | REVERSE_DOUBLE_TAP |
| `0x04` | LEFT_PINCH | `0x0E` | PINCH_ROLL_LEFT |
| `0x05` | RIGHT_PINCH | `0x0F` | PINCH_ROLL_RIGHT |
| `0x06` | UP_PINCH | `0x10` | PINCH_IN |
| `0x07` | DOWN_PINCH | `0x11` | PINCH_OUT |
| `0x08` | REVERSE_LEFT_PINCH | `0x12` | SHORT_TAP |
| `0x09` | REVERSE_RIGHT_PINCH | `0x13` | REVERSE_TAP |

### 6.4 Derived-signal configuration [RE]

| Signal | Configurable items |
|--------|--------------------|
| Gesture | Confidence up/down thresholds, inference model (`Basic` / `BasicWithoutQuaternion` / `NeuralClicker` / `Embedded`) |
| Pressure | `PressureSmoothing`; related sensor FSR (force-sensitive resistor) |
| Navigation | Pointer speed X/Y (0–255), 8-direction + reverse enum |
| Neural | `NEURAL_OUTPUT_SNC_A/B`; 9 TFLite models (neural_clicker, snc_attention, tap_stage2, …) |

---

## 7. MESSAGE channel — `0xfff6`

`byte[0]` is the message type.

| byte[0] | Type | Contents |
|---------|------|----------|
| `0x13` (19) | battery_information | SOC, voltage, capacity, current, cycle count, time remaining, full capacity, PMIC state (22 bytes) |
| `0x03` (3) | air_mouse_calibration_finished | Air-mouse calibration done |
| `0x06` (6) | secondary_device_paired | Secondary device paired |
| `0x07` (7) | secondary_device_unpaired | Unpaired |
| `0x08` (8) | secondary_device_changed | Active device switched |
| `0x09` (9) | secondary_device_action_failed | Action failed |
| `0x0a` (10) | secondary_device_list_init | Device list initialized |

### Detailed battery (type `0x13`) example

```
characteristic: 0xfff6
received bytes (hex):  13 4b 05  a0 a8  c8 00  d0 ff  ...   (22 bytes total)
                       │  │  │   └voltage└capacity└current(signed) ...
                       │  │  └ age=5
                       │  └ SOC=0x4b=75%
                       └ type=0x13 (BATTERY_INFORMATION)

conversion factors:
  voltage  = raw × 7.8125e-5  [V]
  capacity = raw × 0.005      [mAh]
  current  = raw × 0.0015625  [mA]  (signed: − discharging / + charging)
  cycles   = raw × 0.01
  time     = raw × 5          [min]
```

---

## 8. Simple notifications & read-only

```
BATTERY  (0x2a19):  4b              → byte[0]=0x4b = 75%
CHARGING (0x2a1a):  7b              → byte[0]==0x7B(123) → charging (otherwise not charging)
FW_VER   (0x2a26):  31 2e 32 2e 33  → UTF-8 "1.2.3"
SERIAL:  right(0x2a25)+left(0x2a27) hex strings parsed to int → serial = left×1,000,000 + right
```

---

## 9. Signal-enable commands (host → device, reference)

Every stream is off by default. Start it by writing to COMMAND (`0xfff1`).

| Operation | Bytes | Prodilink API |
|-----------|-------|---------------|
| Enable SNC | `06 00 01` | `enable_snc()` |
| Disable SNC | `06 00 00` | `disable_snc()` |
| Enable accelerometer | `07 03 01` | `enable_imu_acc()` |
| Enable gyroscope | `07 02 01` | `enable_imu_gyro()` |
| Enable quaternion | `07 01 01` | `enable_imu_quaternion()` |
| Enable gestures | `07 08 01` | `enable_gestures()` |
| Enable pressure | `06 01 01` | `enable_pressure()` |
| Enable navigation | `07 07 01` | `enable_navigation()` |
| Set sample type | `22 PP` | `set_sample_type("16bit"/"24bit")` |
| Request general status | `75 01` | `refresh_status()` |

---

## 10. License tiers (reference) [RE]

A gate that exists only inside the official SDK. **It does not exist in the device firmware, which accepts all commands**, so Prodilink acquires data directly via BLE writes.

`Main` / `RawData` (raw EMG/IMU equivalent) / `TensorFlowData` / `DoubleTap` / `BasicModel`

---

## 11. Verification notes (to prevent misunderstanding)

1. **Sampling rates (SNC ~1000 Hz / IMU ~100 Hz) are nominal.** They come from the companion-SDK protocol definition; actual rates can vary with device, connection conditions, and the number of active streams. If you need exact rates, measure them from received timestamps.
2. **Raw byte width, normalization, RMS, and physical-unit conversion are outputs of layer B (the official SDK native lib).** The raw bytes Prodilink receives (layer A) are count values and do not include these.
3. **Timestamps come in two kinds**: device internal time (`*_TS`) and app receive time (`*_APP_TS`). With Prodilink's `on_raw_*` you must attach a host receive time yourself.
4. **The exact SNC byte layout (channel multiplexing, byte-width boundaries) is not decoded.** `on_raw_snc` only forwards raw bytes.

---

## 12. Source links

### Official / FCC / Press
- [Mudra Link product page (specs)](https://mudra-band.com/products/mudra-link)
- [How It Works (signal processing)](https://mudra-band.com/pages/howitworks)
- [Mudra Link main page](https://mudra-band.com/pages/mudra-link-main)
- [FCC certification (RF specs)](https://fcc.report/FCC-ID/2A5CU-MUDRABAND)
- [FCC ID (test reports / photos)](https://fccid.io/2A5CU-MUDRABAND)
- [UploadVR review](https://www.uploadvr.com/mudra-link-is-a-200-meta-neural-wristband-alternative-for-any-device/)
- [Android Headlines announcement](https://www.androidheadlines.com/2024/09/mudra-link-gesture-controlled-neural-interface-wristband-unveiled.html)
- [NCBI PMC10221799 — wrist-worn sEMG (academic background, not product-specific)](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC10221799/)

### In-repository (reverse engineering)
- `re/protocol/PROTOCOL_REFERENCE.md` — full protocol spec
- `re/companion_app/skill_extracted/mudra-skill/references/agent_protocol.live.v2.json` — signal data spec (frequency, value range, 3 nerves)
- `re/android_sdk/decompiled/sources/mudraAndroidSDK/enums/RecordingDataType.java` — recorder 32-channel definition
- `re/android_sdk/decompiled/sources/mudraAndroidSDK/model/MudraDevice.java` — callback signatures (metrics)
- `re/python_sdk/mudra_sdk/models/computation_wrapper.py` — native compute-layer wrapper
- `prodilink/protocol.py` — Prodilink's parser implementation (IMU 12B/int16, COMMAND tags, battery factors)
