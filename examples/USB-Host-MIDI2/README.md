# [ESP32_Host_MIDI](../..) | USB Host (MIDI 2.0 / UMP)

## USB-Host-MIDI2

USB host that receives raw Universal MIDI Packets (UMP) from a MIDI 2.0 device
and decodes Channel Voice (MIDI 2.0 and MIDI 1.0) to Serial. Mirrors the TinyUSB
`midi2_host` example, on the native ESP-IDF USB host. Packets split across USB
transfers are reassembled; internal negotiation messages are not forwarded.

## Build

Arduino IDE: Board ESP32-S3 (USB host), USB Mode "Hardware CDC and JTAG". Or
arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/USB-Host-MIDI2
```

## Hardware

A USB MIDI 2.0 device on the host port (Teensy, Daisy Seed, RP2040, etc.). A
MIDI 1.0 device is received too, via the MIDI bytes callback.

## Validation

Open the Serial Monitor; play notes on the device. MIDI 2.0 Channel Voice prints
decoded (`NoteOn n=.. vel=....`), and other UMP types print as hex.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
