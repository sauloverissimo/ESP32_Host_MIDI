# [ESP32_Host_MIDI](../..) | Apple MIDI / RTP-MIDI (WiFi)

## RTP-MIDI-WiFi

Connects to WiFi and exposes the ESP32 as an RTP-MIDI (AppleMIDI) device, with a
didactic display: IP and peer count, sequence name/step, note names, raw MIDI
bytes, and a mini piano. macOS/iOS auto-discover it on the network.

## Build

Requires **AppleMIDI** (lathoub, v3.x), **MIDI Library** (Francois Best), and
**LovyanGFX**. Arduino IDE: Board T-Display-S3 (ESP32-S3), Partition Scheme
"Huge App (3MB)". Or arduino-cli:

```bash
arduino-cli lib install AppleMIDI "MIDI Library" LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3:PartitionScheme=huge_app --library . examples/RTP-MIDI-WiFi
```

Not compiled in CI (external library dependencies).

## Hardware

T-Display-S3 (built-in display). Two buttons: cycle sequence and play/stop. Set
`WIFI_SSID` / `WIFI_PASS` in `mapping.h`.

## Validation

On macOS: Audio MIDI Setup, Network, add the discovered session. The selected
sequence plays out to the DAW; the display mirrors what is being sent.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
