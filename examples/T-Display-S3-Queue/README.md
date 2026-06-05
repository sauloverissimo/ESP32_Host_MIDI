# [ESP32_Host_MIDI](../..) | USB Host + BLE MIDI

## T-Display-S3-Queue

Shows the live MIDI event queue and active notes on the T-Display-S3 ST7789
screen. Receives from both USB Host and BLE. Useful for debugging and visualizing
incoming MIDI in detail.

## Build

Arduino IDE: Board T-Display-S3 (ESP32-S3), Partition Scheme "Huge App (3MB)"
(USB + BLE + display exceeds the default partition), LovyanGFX library. Or
arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3:PartitionScheme=huge_app --library . examples/T-Display-S3-Queue
```

## Hardware

T-Display-S3 (built-in ST7789). A USB MIDI device on the host port and/or a BLE
MIDI central. Button 2 clears the queue.

## Validation

Play notes from a USB or BLE MIDI source; the screen lists recent events
(index, channel0, status, note, velocity7, chord) and the active-note set.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
