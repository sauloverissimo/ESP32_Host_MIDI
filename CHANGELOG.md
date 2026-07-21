# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [7.1.0] - Unreleased

### Fixed

- USB sources are guarded by a feature macro so they compile only on targets
  with USB-OTG (ESP32-S2, S3, P4). Reported by @tgmaxx in discussion #28,
  diagnosed by @Samielakkad.
- BLE sources are guarded the same way, compiling only where a Bluetooth stack
  is present. The probe is `CONFIG_BT_ENABLED`, not `CONFIG_SOC_BT_SUPPORTED`,
  so the P4 keeps BLE through its ESP-Hosted co-processor.

### Added

- Selective routing: `MIDITransport::name()`, `MIDIMessage::source`, an optional
  `MIDITransport* target` on all `send*()` methods (defaults to broadcast), and
  `getTransportCount()` / `getTransport(index)`.
- `BLEClientConnection`, a BLE central transport that complements the peripheral
  `BLEConnection`.
- `MIDI-Router` example.
- CI matrix: six chips (ESP32, S2, S3, P4, C3, C6) across every dependency-free
  transport plus the examples, 60 compilations per run. Probe sketches live in
  `extras/ci/probes/`, one per transport, so per-chip build coverage is explicit.
- `test_usb_send` added to CI; native suite now 341 assertions.

[7.1.0]: https://github.com/sauloverissimo/ESP32_Host_MIDI/compare/v7.0.0...v7.1.0
