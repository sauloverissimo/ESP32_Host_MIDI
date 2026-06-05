# [ESP32_Host_MIDI](../..) | Piano Flow (midi2flow)

## T-Display-S3-Piano-Flow

A 25-key piano visualizer driven by gingoduino's `midi2flow` engine. It is
source-agnostic: a USB MIDI 1.0 keyboard, a USB MIDI 2.0 device, and BLE MIDI
all flow through the same interpreter, which names the chord (with inversion)
and tracks note durations in real time. The existing PianoDisplay and PCM5102A
synth are reused; only the analysis engine changes (from MIDIHandler to the
live UMP stream of midi2flow).

`USBMIDI2Connection` handles both MIDI 1.0 (Alt 0) and MIDI 2.0/UMP (Alt 1) on
the host port; `GingoFlowAdapter` normalizes every source to UMP and feeds the
flow. Chord names use `GingoChord::identifyFromMidi`, so inversions show as the
bass slash (a 1st inversion C major reads `CM/E`).

## Build

Requires LovyanGFX and Gingoduino 0.6.0+ (the version that ships `midi2flow`).
Arduino IDE: Board T-Display-S3 (ESP32-S3), Partition Scheme "Huge App (3MB)".
Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX Gingoduino
arduino-cli compile -b esp32:esp32:esp32s3:PartitionScheme=huge_app --library . examples/T-Display-S3-Piano-Flow
```

Not compiled in CI (external libraries).

The host-only logic (MIDI 1.0 to UMP conversion, the flow-derived display
state) has unit tests under `tests/`, built with g++ (no hardware):

```bash
g++ -std=c++11 -Wall -Wextra -I examples/T-Display-S3-Piano-Flow \
    examples/T-Display-S3-Piano-Flow/tests/test_midi1_to_ump.cpp -o /tmp/t && /tmp/t
```

## Hardware

T-Display-S3 (built-in display), a MIDI source on the USB host port (a USB MIDI
1.0 keyboard or a USB MIDI 2.0 device), and a PCM5102A I2S DAC for audio.

## Validation

Play a MIDI 1.0 keyboard: keys light in real time and the chord name shows with
its inversion (play C-E-G, then E-G-C, and read `CM` then `CM/E`). Plug a USB
MIDI 2.0 device and the same interpretation runs unchanged. The DAC sounds the
notes.
