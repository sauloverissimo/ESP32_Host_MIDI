# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [7.1.0] - 2026-07-29

### Fixed

- USB sources are guarded by a feature macro so they compile only on targets
  with USB-OTG (ESP32-S2, S3, P4). Reported by @tgmaxx in discussion #28,
  diagnosed by @Samielakkad.
- BLE sources are guarded the same way, compiling only where a Bluetooth stack
  is present. The probe is `CONFIG_BT_ENABLED`, not `CONFIG_SOC_BT_SUPPORTED`,
  so the P4 keeps BLE through its ESP-Hosted co-processor.
- BLE MIDI input splits batched notifications into individual messages, on both
  the server (`BLEConnection`) and the client (`BLEClientConnection`).

### Added

- Selective routing: `MIDITransport::name()`, `MIDIMessage::source`, an optional
  `MIDITransport* target` on all `send*()` methods (no target sends via the first
  transport that accepts), and `getTransportCount()` / `getTransport(index)`.
- `BLEClientConnection`, a BLE central transport that complements the peripheral
  `BLEConnection`. Sending to devices that only accept `WRITE_NO_RESPONSE` comes
  from @al177 (PR #24).
- `MIDI-Router` (ESP32-S3, USB Host to DIN-5) and `MIDI-Router-P4` (ESP32-P4,
  USB Host with BLE) examples.
- CI matrix: six chips (ESP32, S2, S3, P4, C3, C6) across every dependency-free
  transport plus the examples, 60 compilations per run. Probe sketches live in
  `extras/ci/probes/`, one per transport, so per-chip build coverage is explicit.
- `test_usb_send` and a BLE-MIDI splitter test added to CI; native suite now
  352 assertions.

[7.1.0]: https://github.com/sauloverissimo/ESP32_Host_MIDI/compare/v7.0.0...v7.1.0
