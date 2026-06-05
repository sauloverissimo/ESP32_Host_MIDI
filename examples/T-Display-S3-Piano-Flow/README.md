# [ESP32_Host_MIDI](../..) | Piano Flow (midi2flow)

## T-Display-S3-Piano-Flow

A live music interpreter on the T-Display S3, driven by gingoduino's `midi2flow`
engine. It is source-agnostic: a USB MIDI 1.0 keyboard and a USB MIDI 2.0/UMP
device on the host port both flow through the same interpreter, which tracks the
held notes, names the current chord with inversion, and measures note durations
in real time.

`USBMIDI2Connection` negotiates MIDI 2.0 (Alt 1) when the device offers it and
falls back to MIDI 1.0 (Alt 0) otherwise:

- MIDI 2.0 device: raw UMP arrives via the UMP callback and goes straight into
  the flow.
- MIDI 1.0 device: MIDI bytes arrive via the MIDI callback and `Midi1ToUmp`
  converts them to UMP MT2 before the same flow.

Chord names use `GingoChord::identifyFromMidi`, so inversions read as a bass
slash: a 1st inversion C major shows `CM/E`. The display also shows which
protocol the connected device negotiated (MIDI 1.0 or MIDI 2.0), the held notes,
the last note's duration, and a mini-piano of the held set.

## Build

Requires LovyanGFX and Gingoduino 0.6.0+ (the version that ships `midi2flow`).
Arduino IDE: Board "ESP32S3 Dev Module", PSRAM "OPI PSRAM", Partition Scheme
"Huge App (3MB)". The full-screen sprite lives in PSRAM, so OPI PSRAM must be on.
Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX Gingoduino
arduino-cli compile -b esp32:esp32:esp32s3:PSRAM=opi,PartitionScheme=huge_app --library . examples/T-Display-S3-Piano-Flow
```

Not compiled in CI (external libraries).

The host-only logic (MIDI 1.0 to UMP conversion, the flow-derived display state)
has unit tests under `tests/`, built with g++ (no hardware). Point `-I` at the
Gingoduino library root and its `src/` so `<Gingoduino.h>` resolves as it does on
Arduino:

```bash
g++ -std=c++11 -Wall -Wextra -I examples/T-Display-S3-Piano-Flow \
    examples/T-Display-S3-Piano-Flow/tests/test_midi1_to_ump.cpp -o /tmp/t && /tmp/t
```

## Hardware

T-Display-S3 (built-in display) and a MIDI source on the USB host port: a USB
MIDI 1.0 keyboard or a USB MIDI 2.0 device.

## Validation

Play a MIDI 1.0 keyboard on the host port: keys light in real time and the chord
name shows with its inversion (play C-E-G, then E-G-C, and read `CM` then
`CM/E`). Plug a USB MIDI 2.0 device and the source line flips to `MIDI 2.0` while
the same interpretation runs unchanged.
