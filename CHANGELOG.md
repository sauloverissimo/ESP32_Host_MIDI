# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [7.3.0] - 2026-09-16

### Changed

- Event indexes are 64-bit: `MIDIEventData::index`, `msgIndex` and
  `chordIndex`, `MIDISysExEvent::index`, and the return and parameter types
  of `lastChord()` / `getChord()` are now `uint64_t`. The documented consume
  pattern (`if (ev.index <= lastIndex) continue`) keeps working for the whole
  uptime of a board; declare `lastIndex` as `uint64_t` starting at 0. README
  and examples updated. Reported by @RuudMulder in issue #33.

### Fixed

- Outgoing SysEx on the USB Host transport is split into the USB-MIDI packet
  sequence the device expects (CIN `0x04` for continuation, `0x05`/`0x06`/
  `0x07` for the final 1, 2 or 3 bytes) and submitted in one transfer, so
  multi-packet messages arrive with their `F7` terminator. Reported by
  @isialdi in issue #31.
- `sendSysEx()` returns `true` only when at least one transport accepted the
  message. A busy endpoint, a message larger than the endpoint buffer or an
  unmounted device now report `false`.

### Tests

- The USB send tests compile against the packet builder extracted from
  `USBConnection.cpp` at build time instead of a hand-kept copy.

[7.3.0]: https://github.com/sauloverissimo/ESP32_Host_MIDI/compare/v7.2.0...v7.3.0

## [7.2.0] - 2026-07-31

### Fixed

- USB Host enumerates again on arduino-esp32 3.3.11, which turns on
  `CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK` in its prebuilt libraries. With
  that feature compiled in, an unset `enum_filter_cb` stops enumeration before
  the client is notified, so no device is ever mounted. `USBConnection::begin()`
  now registers a filter that accepts every device. The registration is guarded
  by the feature macro, so cores that predate it are unaffected. Reported by
  @markeby in issue #30; same regression as espressif/arduino-esp32#12778 and
  espressif/arduino-esp32#12783.
- `BLEClientConnection` builds on cores older than 3.3.0 again, restoring the
  `>= 3.0.0` board package requirement the README states. It held the scanned
  peripheral address in a default-constructed `BLEAddress`, a constructor that
  arrived with the NimBLE support in core 3.3.0. The scan-to-connect handoff
  now carries the address in textual form, a representation that is stable
  across cores and BLE stacks, with the address type preserved on cores that
  expose one. Validated on hardware: ESP32-S3 central connecting to an
  ESP32-C6 peripheral on core 3.3.11.

[7.2.0]: https://github.com/sauloverissimo/ESP32_Host_MIDI/compare/v7.1.0...v7.2.0

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
