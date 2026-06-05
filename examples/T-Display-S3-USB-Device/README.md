# [ESP32_Host_MIDI](../..) | USB Device + BLE

## T-Display-S3-USB-Device

The ESP32-S3 is a USB MIDI device for the computer while also receiving MIDI from
a BLE source (iPhone, iPad, BLE keyboard). The display shows USB/BLE status, an
event log and counters.

## Build

Requires LovyanGFX. Arduino IDE: Board T-Display-S3 (ESP32-S3), USB Mode
"USB-OTG (TinyUSB)". Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3:USBMode=default --library . examples/T-Display-S3-USB-Device
```

Not compiled in CI (external display library).

## Hardware

T-Display-S3 (built-in display). USB to the computer; a BLE MIDI source. Device
name via USB_MIDI_DEVICE_NAME in mapping.h.

## Validation

The board appears as a MIDI port in the DAW; BLE notes are forwarded to it.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
