# [ESP32_Host_MIDI](../..) | MIDI Router

## MIDI-Router

Selective MIDI routing between transports. A USB MIDI controller and a BLE
foot controller (BLE Client) feed the handler; everything is forwarded only
to a synth app connected over BLE (BLE Server), with no echo back to the
input devices.

Demonstrates the routing API:

- `event.source`: the transport that received each event
- `name()` / `setName()`: identify transports in routing logic
- `sendNoteOn(..., target)`: send to one transport instead of broadcasting
- `getTransportCount()` / `getTransport(i)`: enumerate registered transports
- `BLEClientConnection`: BLE Central role, scans and connects to a BLE MIDI
  peripheral (multiple instances supported)

## Build

Arduino IDE: Board ESP32-S3, USB Mode: default, Serial Monitor 115200. Or arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/MIDI-Router
```

## Hardware

- ESP32-S3 (or S2/P4) with the OTG port wired for USB Host (VBUS to the device)
- A USB MIDI controller on the OTG port
- A BLE MIDI peripheral (foot controller) for the BLE Client to find
- A synth app (iPad/phone/desktop) connecting to "ESP32 MIDI Router" over BLE

## Validation

1. Plug the USB controller and play: notes appear on the synth app.
2. Pair the foot controller: its CCs arrive on the synth remapped to channel 16.
3. Nothing is echoed back to the controller or the foot controller.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
