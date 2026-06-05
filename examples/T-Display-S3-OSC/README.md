# [ESP32_Host_MIDI](../..) | OSC

## T-Display-S3-OSC

Bidirectional OSC <-> MIDI bridge: a USB keyboard is forwarded as OSC/UDP to
Max/MSP, Pure Data or SuperCollider, and incoming OSC is shown and forwarded as
MIDI. The display shows WiFi/IP, ports, an event log and counters.

## Build

Requires the OSC library (CNMAT) and LovyanGFX. Arduino IDE: Board T-Display-S3
(ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install OSC LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-OSC
```

Not compiled in CI (external libraries).

## Hardware

T-Display-S3 (built-in display). Set WIFI_SSID/WIFI_PASS/OSC_TARGET_IP in
mapping.h. A USB MIDI keyboard on the host port.

## Validation

In Max/MSP `[udpreceive 8000]` receives `/midi/noteon` etc.; OSC sent back is
displayed and forwarded as MIDI.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
