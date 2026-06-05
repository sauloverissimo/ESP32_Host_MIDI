# [ESP32_Host_MIDI](../..) | UART / DIN-5 (dual)

## P4-Dual-UART-MIDI

Two simultaneous MIDI DIN-5 ports on the ESP32-P4, which has 5 hardware UARTs.
Both ports share one `MIDIHandler` and produce events in the same queue.

## Build

Arduino IDE: Board ESP32-P4. Or arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32p4 --library . examples/P4-Dual-UART-MIDI
```

## Hardware

MIDI IN needs an optocoupler per port (e.g. 6N138 or TLP2361); MIDI OUT uses two
220R resistors per port. Adjust the pin defines at the top of the sketch.

```
MIDI IN  port 1/2: DIN-5 pin 5 -- 220R -- optocoupler -- MIDIx_RX_PIN
MIDI OUT port 1/2: MIDIx_TX_PIN -- 220R -- DIN-5 pin 5
```

## Validation

Play into either MIDI IN; the Serial Monitor prints decoded events from both
ports interleaved in one queue.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
