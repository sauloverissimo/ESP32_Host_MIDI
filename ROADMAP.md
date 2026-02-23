# ROADMAP — ESP32_Host_MIDI

## Current State (v4.0.0)

- Full support for ESP32-S3 (USB + BLE + ESP-NOW + PSRAM + dual-core)
- Compile-time conditional: USB and BLE are automatically enabled/disabled based on target chip
- ESP32 family chips with BLE (ESP32, C3, C6, H2) can use BLE MIDI only
- ESP32-S2 and ESP32-P4 can use USB Host MIDI only
- ESP-NOW available on all ESP32 variants with WiFi
- **Transport Abstraction Layer** — `MIDITransport` interface decouples MIDIHandler from transport specifics. USB and BLE are built-in; custom transports plug in via `addTransport()`
- **ESP-NOW transport** — ultra-low latency (~1-5ms), broadcast/unicast, bidirectional
- **BLE MIDI is bidirectional** — send MIDI to connected DAW/app via `sendNoteOn()`, `sendNoteOff()`, `sendControlChange()`, `sendProgramChange()`, `sendPitchBend()`, `sendRaw()`
- **All transports use the same ring buffer architecture** — thread-safe spinlock, `task()` drains from main loop
- Optional Gingoduino integration via GingoAdapter.h
- Compatible with Arduino IDE, PlatformIO, and ESP-IDF (Arduino component)

---

## Phase 1 — ESP32 Ecosystem Hardening

**Goal:** Ensure robust operation across all ESP32 variants.

- [x] **BLE ring buffer** — BLE now uses the same ring buffer + spinlock architecture as USB. BLE callbacks enqueue to the buffer; `task()` drains on the main loop.
- [x] **BLE MIDI output** — BLE is bidirectional. ESP32 sends NOTIFY to the connected central (app/DAW).
- [ ] **Single-core profiling** — Profile and optimize MIDI on ESP32-C3/C6/H2 (single-core). Ring buffer is in place; remaining work is documenting minimum `loop()` timing requirements.
- [ ] **ESP32-P4 validation** — Test USB Host MIDI on ESP32-P4 (dual-core RISC-V with USB-OTG). Verify ESP-IDF USB Host API compatibility.
- [ ] **PSRAM fallback** — History buffer already falls back to heap `malloc()` when PSRAM is unavailable. Validate on chips without PSRAM (ESP32-C3, ESP32-H2).

---

## Phase 2 — Transport Abstraction Layer

**Goal:** Decouple MIDIHandler from ESP32-specific transport, enabling portability and extensibility.

- [x] **`MIDITransport` interface** — Abstract base class with `task()`, `isConnected()`, `sendMidiMessage()`, and callback registration via function pointers (`setMidiCallback`, `setConnectionCallbacks`).
- [x] **USBConnection and BLEConnection** refactored to implement `MIDITransport`. Removed internal subclasses from MIDIHandler.
- [x] **MIDIHandler transport array** — `MIDITransport* transports[4]` with `addTransport()`. Built-in transports auto-registered in `begin()`.
- [x] **ESP-NOW transport** — `ESPNowConnection` class. Ultra-low latency (~1-5ms), broadcast/unicast, bidirectional. Registered via `addTransport()`.
- [ ] **RTP-MIDI (WiFi) transport** — Network MIDI over UDP, compatible with macOS, Ableton, and iOS via AppleMIDI session protocol. Candidate approach: wrap [Arduino-AppleMIDI-Library](https://github.com/lathoub/Arduino-AppleMIDI-Library) (MIT, 321 stars) as a `MIDITransport`.

---

## Phase 3 — Raspberry Pi Pico / RP2040 / RP2350

**Goal:** Support Raspberry Pi Pico and Pico 2 for USB Host MIDI.

**Challenges:**
- RP2040/RP2350 use **TinyUSB** for USB Host (not ESP-IDF USB Host API)
- No native BLE (Pico W has WiFi via CYW43, but BLE support is limited)
- No PSRAM (limited to 264KB/520KB SRAM)
- Different FreeRTOS port (if using arduino-pico core) or no RTOS (bare metal)

**Steps:**
- [ ] Implement `TinyUSBMIDITransport` using TinyUSB Host API
- [ ] Port ring buffer to use RP2040 multicore primitives (`mutex`, `spin_lock`)
- [ ] Test with arduino-pico core (Earle Philhower) and official Arduino Mbed core

---

## Phase 4 — Teensy 4.x

**Goal:** Support Teensy 4.0/4.1 for USB Host MIDI.

**Challenges:**
- Teensy uses **USBHost_t36** library (proprietary USB Host stack)
- Already has native MIDI Host support via `USBHost_t36/MIDIDevice.h`
- No BLE natively (Teensy 4.1 can use external BLE modules)
- Has C++ STL support via Teensyduino

**Steps:**
- [ ] Implement `TeensyUSBMIDITransport` wrapping `USBHost_t36::MIDIDevice`
- [ ] Evaluate whether MIDIHandler adds value on Teensy (since USBHost_t36 already parses MIDI)

---

## Phase 5 — Daisy Seed (STM32H7)

**Goal:** Support Electro-Smith Daisy Seed for USB Host MIDI.

**Challenges:**
- Uses STM32 HAL for USB Host (completely different API)
- Runs libDaisy + DaisySP (custom SDK, not Arduino by default)
- Arduino support exists via STM32duino but USB Host is limited
- Has DSP capabilities — ideal target for audio + MIDI

**Steps:**
- [ ] Implement `STM32USBMIDITransport` using STM32 HAL USB Host
- [ ] Consider libDaisy native integration (non-Arduino)

---

## Phase 6 — Seeed XIAO nRF52840 / Arduino Nano 33 BLE

**Goal:** Support nRF52840-based boards for BLE MIDI.

**Challenges:**
- Uses **ArduinoBLE** library (not ESP32 BLE stack)
- Different BLE API: `BLEService`, `BLECharacteristic` from ArduinoBLE
- No USB Host capability
- Has C++ STL support (Mbed OS / Adafruit nRF52 core)

**Steps:**
- [ ] Implement `ArduinoBLEMIDITransport` wrapping ArduinoBLE API
- [ ] Validate BLE MIDI service UUID compatibility

---

## Phase 7 — MIDI 2.0

**Goal:** Add MIDI 2.0 (UMP — Universal MIDI Packet) support.

**What MIDI 2.0 adds:**
- 32-bit resolution for velocity, pitch bend, CC (vs 7/14-bit in MIDI 1.0)
- Per-note controllers and articulation
- Bidirectional protocol negotiation (MIDI-CI)
- New message types: Per-Note Management, Flex Data, UMP streams

**Steps:**
- [ ] Research USB-MIDI 2.0 class driver requirements (different from USB-MIDI 1.0)
- [ ] Extend `MIDIEventData` with optional high-resolution fields
- [ ] Add MIDI-CI capability inquiry support
- [ ] Maintain backward compatibility with MIDI 1.0 devices

---

## Contributing

If you're interested in helping with any of these phases, especially porting to new platforms, please open an issue or PR on GitHub. Hardware donations for testing are also welcome!
