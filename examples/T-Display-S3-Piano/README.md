# [ESP32_Host_MIDI](../..) | USB Host + Piano / Synth

## T-Display-S3-Piano

25-key piano visualizer with music-theory analysis (Gingoduino) and an onboard
PCM5102A synth. Designed for MIDI-25 keyboards (e.g. Arturia Minilab 25). Uses a
full-screen PSRAM sprite for anti-tearing.

## Build

Requires LovyanGFX and the Gingoduino library. Arduino IDE: Board T-Display-S3
(ESP32-S3), Partition Scheme "Huge App (3MB)". Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX Gingoduino
arduino-cli compile -b esp32:esp32:esp32s3:PartitionScheme=huge_app --library . examples/T-Display-S3-Piano
```

Not compiled in CI (external libraries).

## Hardware

T-Display-S3 (built-in display), a USB MIDI keyboard, and a PCM5102A I2S DAC for
audio.

## Validation

Play the keyboard; keys light in real time with theory analysis and the DAC
sounds the notes.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
