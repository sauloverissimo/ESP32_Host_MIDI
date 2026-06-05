# [ESP32_Host_MIDI](../..) | USB Host + AM_MIDI2.0Lib adapter

## AM-MIDI2-Adapter

Bridges MIDI 1.0 received over USB (or BLE) into Andrew Mee's AM_MIDI2.0Lib
`umpProcessor` as UMP, exposing MIDI 2.0 high-resolution values while the legacy
`midiHandler.getQueue()` API keeps working.

## Build

Requires the **AM_MIDI2.0Lib** library (Arduino Library Manager: "AM_MIDI2.0Lib",
or PlatformIO `midi2-dev/AM_MIDI2.0Lib`). Arduino IDE: Board ESP32-S3 (USB) or
any ESP32 (BLE). Or arduino-cli:

```bash
arduino-cli lib install "AM MIDI2.0Lib"   # or install from source
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/AM-MIDI2-Adapter
```

Not compiled in CI (external library dependency).

## Hardware

A USB MIDI device on the host port (or a BLE MIDI source). Output is on Serial.

## Validation

Play notes; the Serial Monitor prints the AM-MIDI2 high-res view (`val32`,
`vel16`) and the legacy MIDI 1.0 view side by side.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
