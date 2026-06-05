# [ESP32_Host_MIDI](../..) | USB Host (SysEx)

## T-Display-S3-SysEx

SysEx monitor: shows received SysEx messages and MIDI events on the display. The
SysEx queue is separate from the normal event queue, so existing `getQueue()`
code is unaffected.

## Build

Requires LovyanGFX. Arduino IDE: Board T-Display-S3 (ESP32-S3). Or arduino-cli:

```bash
arduino-cli lib install LovyanGFX
arduino-cli compile -b esp32:esp32:esp32s3 --library . examples/T-Display-S3-SysEx
```

Not compiled in CI (external display library).

## Hardware

T-Display-S3 (built-in display). A USB MIDI device. BTN1 sends an Identity
Request; BTN2 clears the SysEx queue.

## Validation

Send SysEx from the device (or press BTN1); messages and events are listed on
screen.

## License

MIT, inherits parent [`ESP32_Host_MIDI` LICENSE](../../LICENSE).
