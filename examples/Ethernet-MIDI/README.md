# [ESP32_Host_MIDI](../..) | Apple MIDI over Ethernet

## Ethernet-MIDI

Exposes the ESP32 as an RTP-MIDI (AppleMIDI) device over wired Ethernet using a
W5x00 SPI module (W5500 recommended) or the ESP32-P4 native MAC. Lower and more
consistent latency than WiFi RTP-MIDI. On ESP32-S3 it also runs USB Host, so a
USB keyboard is forwarded to the DAW over Ethernet in real time.

## Build

Requires the **AppleMIDI** library (lathoub, v3.x) and an **Ethernet** library
(built-in, or Ethernet_Generic for W5500). Arduino IDE: Board ESP32-S3 or
ESP32-P4. Or arduino-cli:

```bash
arduino-cli lib install AppleMIDI Ethernet
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/Ethernet-MIDI
```

Not compiled in CI (external library dependencies).

## Hardware

W5500 module on SPI (see `mapping.h` for pins); set the MAC address and IP in
`mapping.h`. ESP32-S3 can also host a USB MIDI device.

## Validation

On macOS: Audio MIDI Setup, Network, click "+", enter the device IP and port
5004, Connect. The ESP32 appears as a network MIDI port; notes flow both ways.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
