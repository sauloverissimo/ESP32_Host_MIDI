# [ESP32_Host_MIDI](../..) | BLE MIDI

## T-Display-S3-BLE-Receiver

Receives MIDI over Bluetooth Low Energy and shows a real-time piano visualizer
on the ST7789 display. Pair with T-Display-S3-BLE-Sender or any BLE MIDI app.

## Build

Requires LovyanGFX. Arduino IDE: Board T-Display-S3 (ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-BLE-Receiver
```

Not compiled in CI (external display library).

## Hardware

T-Display-S3 (built-in display). A BLE MIDI source (phone app, another board).

## Validation

Connect a BLE MIDI source; played notes light up on the on-screen piano.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
