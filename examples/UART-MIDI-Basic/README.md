# [ESP32_Host_MIDI](../..) | UART / DIN-5

## UART-MIDI-Basic

Minimal serial MIDI 1.0 example. Receives MIDI from a DIN-5 / UART MIDI IN and
prints decoded events to Serial; optionally sends a NoteOn/NoteOff pair on a TX
pin.

## Build

Arduino IDE: Board ESP32-S3 (or any ESP32), Serial Monitor 115200. Or arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/UART-MIDI-Basic
```

## Hardware

MIDI IN via opto-isolator to `MIDI_RX_PIN`; optional MIDI OUT on `MIDI_TX_PIN`
(set to -1 to disable). Adjust the pin defines at the top of the sketch.

```
MIDI IN:
  DIN-5 pin 5 -- 220R -- optocoupler anode (e.g. 6N138 or TLP2361)
  DIN-5 pin 2 -- GND
  optocoupler output -- MIDI_RX_PIN
MIDI OUT (optional):
  MIDI_TX_PIN -- 220R -- DIN-5 pin 5
  3.3V        -- 220R -- DIN-5 pin 4
  DIN-5 pin 2 -- GND
```

## Validation

Play notes into MIDI IN; the Serial Monitor prints NoteOn/NoteOff/CC decoded
with v6 fields (`statusCode`, `channel0`, `noteNumber`, `velocity7`).

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
