# [ESP32_Host_MIDI](../..) | MIDI Router

## MIDI-Router

Selective MIDI routing between transports on the ESP32-S3. A USB MIDI controller
feeds the handler, and every event it sends is forwarded to a classic DIN-5
output over UART, using the per-transport send API. Nothing is echoed back.

USB Host and UART are independent peripherals, so they run together with no
contention. On the ESP32-S3, USB Host cannot share the chip with a radio
transport (BLE / Wi-Fi / ESP-NOW): the USB PHY is shared with the radio. For a
router that combines USB Host and BLE, use the ESP32-P4, see the MIDI-Router-P4
example.

Demonstrates the routing API:

- `event.source`: the transport that received each event
- `name()`: identify transports in routing logic
- `sendNoteOn(..., target)`: send to one specific transport (no target sends via
  the first transport that accepts)
- `getTransportCount()` / `getTransport(i)`: enumerate registered transports

## Build

Arduino IDE: Board ESP32-S3, USB Mode: default, Serial Monitor 115200. Or arduino-cli:

```bash
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/MIDI-Router
```

## Hardware

- ESP32-S3 (or S2/P4) with the OTG port wired for USB Host (VBUS to the device)
- A USB MIDI controller on the OTG port
- A DIN-5 MIDI output circuit on UART1 (see the UART-MIDI-Basic example for the
  wiring; adjust `DIN_RX_PIN` / `DIN_TX_PIN` to your board)
- A synth or sound module on the DIN-5 output

## Validation

1. Plug the USB controller and play: notes appear at the DIN-5 output.
2. Only events from the USB controller are forwarded; nothing is echoed back.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
