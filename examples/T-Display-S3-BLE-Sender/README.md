# [ESP32_Host_MIDI](../..) | BLE MIDI

## T-Display-S3-BLE-Sender

Sends pre-programmed musical sequences over BLE MIDI to a receiver (the
T-Display-S3-BLE-Receiver example or any BLE MIDI app), with a didactic display
showing connection state, sequence, and notes.

## Build

Requires LovyanGFX. Arduino IDE: Board T-Display-S3 (ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-BLE-Sender
```

Not compiled in CI (external display library).

## Hardware

T-Display-S3 (built-in display). Buttons cycle the sequence and play/stop.

## Validation

Pair a BLE MIDI receiver; the selected sequence plays out and the display
mirrors it.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
