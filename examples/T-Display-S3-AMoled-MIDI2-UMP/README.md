# [ESP32_Host_MIDI](../..) | MIDI 2.0 / UMP (WiFi UDP)

## T-Display-S3-AMoled-MIDI2-UMP

Two T-Display-S3 AMOLED boards exchange MIDI 2.0 (UMP) over WiFi UDP. The RM67162
AMOLED shows received packets, peers, last note (16-bit velocity), an event log
and counters.

## Build

Requires LovyanGFX. Arduino IDE: Board ESP32-S3 (T-Display AMOLED), Partition
Scheme "Huge App (3MB)". Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3:PartitionScheme=huge_app --library . examples/T-Display-S3-AMoled-MIDI2-UMP
```

Not compiled in CI (external display library).

## Hardware

T-Display-S3 AMOLED (RM67162). Set WIFI_SSID/WIFI_PASS and each board's PEER_IP
in mapping.h. BOOT button sends a test note / cycles velocity preset.

## Validation

Flash both boards, set PEER_IP to each other, press BOOT: the other board's
display reacts and the UMP event log scrolls.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
