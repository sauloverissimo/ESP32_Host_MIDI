# [ESP32_Host_MIDI](../..) | USB Host (MIDI 1.0)

## USB-Host-Send-Test

Integration test: sends every standard MIDI message type to a connected USB
device and reports pass/fail per `sendMidiMessage()` call on Serial. Runs once
after device detection.

## Build

Arduino IDE: Board ESP32-S3 (USB host), USB Mode "Hardware CDC and JTAG". Or
arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/USB-Host-Send-Test
```

## Hardware

A USB MIDI device (keyboard, synth, interface) on the host port. Output is on
Serial at 115200.

## Validation

Open the Serial Monitor; after the device is detected the test sends each
message type and prints a pass/fail line for each. All should pass with a
compliant device.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
