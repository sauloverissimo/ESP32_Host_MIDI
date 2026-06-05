# [ESP32_Host_MIDI](../..) | USB Host + Music Theory

## T-Display-S3-Gingoduino

USB Host MIDI with live music-theory analysis via Gingoduino: note name and
frequency, interval identification, chord identification, and harmonic-field
deduction, shown on the ST7789 display and Serial.

## Build

Requires LovyanGFX and the Gingoduino library. Arduino IDE: Board T-Display-S3
(ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX Gingoduino
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-Gingoduino
```

Not compiled in CI (external libraries).

## Hardware

T-Display-S3 (built-in display). A USB MIDI keyboard on the host port.

## Validation

Play one, two, or more notes; the display shows note/interval/chord and, after a
few chords, the harmonic field.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
