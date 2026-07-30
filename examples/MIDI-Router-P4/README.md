# [ESP32_Host_MIDI](../..) | MIDI Router (P4)

## MIDI-Router-P4

Selective MIDI routing with USB Host and BLE running at the same time, on the
ESP32-P4. A USB MIDI controller and a BLE foot controller (BLE Client) feed the
handler; everything is forwarded only to a synth app connected over BLE (BLE
Server), with no echo back to the input devices.

Why the P4 and not the S3: the ESP32-S3 shares one USB PHY between USB-OTG and the
radio, so USB Host and BLE cannot run together there (the USB host goes silent
once BLE comes up). The P4 has no native Bluetooth, BLE runs on an ESP32-C6
co-processor over SDIO, so nothing contends for the USB PHY and USB Host + BLE
coexist. For an S3, route USB Host to a DIN-5 output instead (see the MIDI-Router
example).

Demonstrates the routing API:

- `event.source`: the transport that received each event
- `name()` / `setName()`: identify transports in routing logic
- `sendNoteOn(..., target)`: send to one specific transport
- `getTransportCount()` / `getTransport(i)`: enumerate registered transports
- `BLEClientConnection`: BLE Central role, scans and connects to a BLE MIDI
  peripheral

## Build

Arduino IDE: Board ESP32-P4. Or arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32p4 --library . examples/MIDI-Router-P4
```

## Hardware

- ESP32-P4 with a USB Host port
- A USB MIDI controller on the host port. The P4 host is High-Speed; a Full-Speed
  device behind a hub needs a Transaction Translator, which the ESP-IDF host stack
  does not implement, so use a High-Speed device or connect directly (no hub).
- A BLE MIDI peripheral (foot controller) for the BLE Client to find
- A synth app (iPad/phone/desktop) connecting to "ESP32 MIDI Router" over BLE

## Validation

1. Plug the USB controller and play: notes appear on the synth app.
2. Pair the foot controller: its CCs arrive on the synth remapped to channel 16.
3. Nothing is echoed back to the controller or the foot controller.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
