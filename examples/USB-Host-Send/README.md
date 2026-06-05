# [ESP32_Host_MIDI](../..) | USB Host (MIDI 1.0)

## USB-Host-Send

USB host that detects a connected MIDI keyboard and then sends MIDI back to it:
a NoteOn/NoteOff pair on detection, and sustain (CC64) from a hardware pedal.
Status LEDs report detection / activity / host state.

## Build

Arduino IDE: Board ESP32-S3 (USB host), USB Mode "Hardware CDC and JTAG". Or
arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/USB-Host-Send
```

## Hardware

- MIDI keyboard on the USB host port (OTG).
- Pedal (momentary, to GND) on `PEDAL_PIN` (sends CC64).
- Status LEDs on `OUT_PIN5` (piano detected), `OUT_PIN6` (blink), `OUT_PIN7`
  (host started).

## Validation

Power up with a USB MIDI keyboard connected. The host-started LED lights; on the
first received message the piano-detected LED lights and a NoteOn/NoteOff is sent
back. Pressing the pedal toggles sustain (CC64). If no device is seen within 10 s
the board restarts.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
