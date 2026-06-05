# [ESP32_Host_MIDI](../..) | USB Host (debug)

## T-Display-S3-Piano-Debug

On-screen MIDI event monitor: shows incoming USB MIDI directly on the ST7789
display (manual tracking vs fillActiveNotes), a color-coded event log, and a
mini-piano bar. Useful to diagnose USB MIDI without a Serial port.

## Build

Requires LovyanGFX. Arduino IDE: Board T-Display-S3 (ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-Piano-Debug
```

Not compiled in CI (external display library).

## Hardware

T-Display-S3 (built-in display). A USB MIDI keyboard on the host port.

## Validation

Play notes; events appear on screen with the active-note set and mini-piano.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
