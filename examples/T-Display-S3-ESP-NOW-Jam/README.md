# [ESP32_Host_MIDI](../..) | ESP-NOW MIDI

## T-Display-S3-ESP-NOW-Jam

Flash the same sketch to two T-Display-S3 boards. Each plays sequences and
broadcasts them over ESP-NOW; the other receives and shows them on a shared
piano visualizer (local vs remote keys color-coded).

## Build

Requires LovyanGFX. Arduino IDE: Board T-Display-S3 (ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-ESP-NOW-Jam
```

Not compiled in CI (external display library).

## Hardware

Two T-Display-S3 boards (no pairing setup; ESP-NOW broadcast).

## Validation

Power both boards; each shows its own notes in one color and the peer's in
another.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
