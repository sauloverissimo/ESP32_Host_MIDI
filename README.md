# ESP32_Host_MIDI

![image](https://github.com/user-attachments/assets/bba1c679-6c76-45b7-aa29-a3201a69b36a)

![Arduino](https://img.shields.io/badge/Arduino-IDE%20%7C%20CLI-00979D?logo=arduino&logoColor=white)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-FF7F00?logo=platformio&logoColor=white)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-Arduino%20Component-E7352C?logo=espressif&logoColor=white)
![License](https://img.shields.io/github/license/sauloverissimo/ESP32_Host_MIDI)
![Version](https://img.shields.io/github/v/release/sauloverissimo/ESP32_Host_MIDI)

**Receive and send MIDI on ESP32 — via USB-OTG, Bluetooth Low Energy, and ESP-NOW.**

ESP32_Host_MIDI turns your ESP32 into a MIDI hub. Plug a USB MIDI keyboard, connect a phone app via Bluetooth, bridge two ESP32s wirelessly with ESP-NOW, or do all at the same time. The library handles the low-level transport and gives you a clean, high-level API to read notes, detect chords, and send MIDI messages back.

```cpp
#include <ESP32_Host_MIDI.h>

void setup() {
    midiHandler.begin();
}

void loop() {
    midiHandler.task();

    // Read what's playing
    auto notes = midiHandler.getActiveNotesVector();  // ["C4", "E4", "G4"]

    // Send MIDI via any connected transport
    midiHandler.sendNoteOn(1, 60, 100);   // Channel 1, Middle C, velocity 100
}
```

---

## What it does

- **USB Host MIDI** — ESP32 acts as USB host. Plug any class-compliant MIDI controller (keyboard, pad, etc.) directly into the ESP32's USB-OTG port.
- **BLE MIDI (bidirectional)** — ESP32 acts as a BLE MIDI peripheral. A phone app or DAW connects to it. Both can send and receive MIDI.
- **ESP-NOW MIDI** — Ultra-low latency wireless MIDI (~1-5ms) between ESP32 devices. No WiFi network needed. Broadcast (no pairing) or unicast (peer-to-peer).
- **Transport abstraction** — `MIDITransport` interface lets you add custom transports (ESP-NOW, RTP-MIDI, serial, etc.) via `addTransport()`. USB and BLE are built-in; others plug in at runtime.
- **MIDI processing** — Parses NoteOn, NoteOff, ControlChange, ProgramChange, PitchBend, ChannelPressure. Converts note numbers to names ("C4"), groups simultaneous notes into chords, tracks active notes.
- **Thread-safe** — All transports use ring buffers with spinlock protection. USB runs on a dedicated FreeRTOS task (Core 0). All MIDI processing happens on the main loop (Core 1), so your code never sees race conditions.

---

## Platform compatibility

### Build systems

| Platform | Compatible? | Notes |
|----------|:-----------:|-------|
| **Arduino IDE** | Yes | Native support. Just install and use. |
| **PlatformIO (Arduino framework)** | Yes | Add to `lib_deps` in `platformio.ini` (see below). |
| **PlatformIO (ESP-IDF + Arduino)** | Yes | Use `framework = arduino, espidf` in `platformio.ini`. |
| **ESP-IDF pure (no Arduino)** | No | Requires `Arduino.h`, `String`, `millis()`, and the ESP32 BLE Arduino library. |

**PlatformIO example:**

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    https://github.com/sauloverissimo/ESP32_Host_MIDI.git
```

> **Other languages** (MicroPython, Rust, TinyGo, Lua, etc.): not compatible. The library is C++ and depends on the Arduino-ESP32 core and ESP-IDF APIs.

### ESP32 chip support

The library uses **compile-time feature detection** to automatically enable or disable USB and BLE based on the target chip. Select the correct board — the library adapts. ESP-NOW is available on all chips.

| Chip | USB Host | BLE | ESP-NOW | Dual-Core | PSRAM | Status |
|------|:--------:|:---:|:-------:|:---------:|:-----:|--------|
| **ESP32-S3** | Yes | Yes | Yes | Yes | Yes | **Recommended** |
| **ESP32-S2** | Yes | No | Yes | No | depends | Supported |
| **ESP32** (classic) | No | Yes | Yes | Yes | depends | Supported |
| **ESP32-C3** | No | Yes | Yes | No | No | Supported |
| **ESP32-C6** | No | Yes | Yes | No | No | Supported |
| **ESP32-H2** | No | Yes | No | No | No | Supported |
| **ESP32-P4** | Yes | No | TBD | Yes | Yes | Supported |

> **Single-core chips** (S2, C3, C6, H2): USB or BLE shares CPU time with `loop()`. Keep `loop()` lightweight and call `midiHandler.task()` frequently.

### Feature detection macros

```cpp
#if ESP32_HOST_MIDI_HAS_USB
  // USB Host is available (ESP32-S2, S3, P4)
#endif
#if ESP32_HOST_MIDI_HAS_BLE
  // BLE MIDI is available (ESP32, S3, C3, C6, H2)
#endif
#if ESP32_HOST_MIDI_HAS_PSRAM
  // PSRAM is available for history buffer
#endif
```

### Other boards

| Board | Compatible? | Reason |
|-------|:-----------:|--------|
| **Arduino Nano ESP32** (ESP32-S3) | Yes | Works natively |
| **Seeed XIAO ESP32-S3** | Yes | Works natively |
| **Seeed XIAO ESP32-C3** | Yes | BLE only |
| **Raspberry Pi Pico** | No | Different USB Host API |
| **Teensy 4.x** | No | Different USB Host API |
| **Arduino Uno / Mega** (AVR) | No | No USB Host, no BLE, no STL |

> See [ROADMAP.md](ROADMAP.md) for plans to support additional platforms.

---

## Getting started

### Installation

1. **Arduino IDE**: Download as ZIP and install via *Sketch > Include Library > Add .ZIP Library*, or clone directly into your Arduino `libraries/` folder.
2. **PlatformIO**: Add the GitHub URL to `lib_deps` in your `platformio.ini` (see above).

### Dependencies

- **ESP32 Arduino Core** 2.0.0+ (includes USB Host and BLE support)
- **[LovyanGFX](https://github.com/lovyan03/LovyanGFX)** 0.4.x+ (only for display examples)
- **[Gingoduino](https://github.com/sauloverissimo/gingoduino)** v0.2.2+ (optional, for music theory analysis)

### Quick start

```cpp
#include <ESP32_Host_MIDI.h>

void setup() {
    Serial.begin(115200);

    MIDIHandlerConfig config;
    config.bleName = "My MIDI Device";    // BLE advertising name
    config.chordTimeWindow = 50;          // 50ms chord grouping window

    midiHandler.begin(config);
}

void loop() {
    midiHandler.task();  // MUST be called every loop iteration

    // Check what's playing
    if (midiHandler.getActiveNotesCount() > 0) {
        Serial.println(midiHandler.getActiveNotes().c_str());  // "{C4, E4, G4}"
    }
}
```

### Board settings (Arduino IDE)

- Board: **ESP32S3 Dev Module** (or your specific board)
- USB Mode: **USB-OTG (TinyUSB)** (required for USB Host)

---

## Receiving MIDI

Both USB and BLE reception work automatically after calling `midiHandler.begin()`. MIDI events are parsed, converted, and made available through the API.

### Reading the event queue

```cpp
const auto& queue = midiHandler.getQueue();
for (const auto& event : queue) {
    Serial.printf("[%s] ch=%d note=%s vel=%d\n",
        event.status.c_str(), event.channel,
        event.noteOctave.c_str(), event.velocity);
}
```

### Active notes

```cpp
// As a formatted string
String notes = midiHandler.getActiveNotes().c_str();  // "{C4, E4, G4}"

// As a vector
auto vec = midiHandler.getActiveNotesVector();  // ["C4", "E4", "G4"]

// As a bool array (best for real-time rendering)
bool active[128];
midiHandler.fillActiveNotes(active);
if (active[60]) { /* middle C is pressed */ }
```

### Chord detection

```cpp
int chord = midiHandler.lastChord(midiHandler.getQueue());
auto noteNames = midiHandler.getChord(chord, midiHandler.getQueue(), {"noteName"});
// noteNames: ["C", "E", "G"]

// Shorthand for the last chord:
auto answer = midiHandler.getAnswer("noteName");
```

### MIDIEventData fields

| Field | Type | Description |
|-------|------|-------------|
| `index` | `int` | Global event counter |
| `msgIndex` | `int` | Links NoteOn/NoteOff pairs |
| `timestamp` | `unsigned long` | Timestamp in ms (`millis()`) |
| `delay` | `unsigned long` | Delta time since previous event |
| `channel` | `int` | MIDI channel (1-16) |
| `status` | `std::string` | `"NoteOn"`, `"NoteOff"`, `"ControlChange"`, `"ProgramChange"`, `"PitchBend"`, `"ChannelPressure"` |
| `note` | `int` | MIDI note number (or CC number) |
| `noteName` | `std::string` | Musical note name ("C", "D#") |
| `noteOctave` | `std::string` | Note with octave ("C4", "D#5") |
| `velocity` | `int` | Velocity (or CC value, program number, pressure) |
| `chordIndex` | `int` | Groups simultaneously pressed notes |
| `pitchBend` | `int` | 14-bit value (0-16383, center = 8192) |

---

## Sending MIDI

Send methods work via any transport that supports sending (BLE, ESP-NOW, or custom transports). All methods return `true` if the message was sent, `false` if no transport is available.

```cpp
// Notes (channel: 1-16)
midiHandler.sendNoteOn(1, 60, 100);    // Channel 1, Middle C, velocity 100
midiHandler.sendNoteOff(1, 60, 0);     // Release

// Control Change
midiHandler.sendControlChange(1, 1, 64);   // CC#1 (Mod Wheel), value 64

// Program Change
midiHandler.sendProgramChange(1, 5);   // Program 5

// Pitch Bend (-8192 to 8191, 0 = center)
midiHandler.sendPitchBend(1, 0);       // Center
midiHandler.sendPitchBend(1, 4096);    // Bend up

// Raw MIDI bytes (status + data, no headers)
uint8_t raw[] = {0x90, 60, 100};
midiHandler.sendRaw(raw, 3);

// Check BLE connection
#if ESP32_HOST_MIDI_HAS_BLE
if (midiHandler.isBleConnected()) {
    // A phone/DAW is connected via BLE
}
#endif
```

### USB-to-BLE bridge example

```cpp
#include <ESP32_Host_MIDI.h>

void setup() {
    midiHandler.begin();
    midiHandler.setRawMidiCallback(onRawMidi);
}

// Forward every incoming MIDI message to BLE
void onRawMidi(const uint8_t* raw, size_t rawLen, const uint8_t* midi3) {
    midiHandler.sendRaw(midi3, 3);
}

void loop() {
    midiHandler.task();
}
```

---

## Transport abstraction

The library uses a `MIDITransport` interface that decouples MIDI processing from specific hardware. USB and BLE are built-in transports registered automatically. You can add custom transports via `addTransport()`.

### Architecture

```
MIDIHandler ──[MIDITransport*]──> USBConnection     (built-in, automatic)
             ──[MIDITransport*]──> BLEConnection     (built-in, automatic)
             ──[MIDITransport*]──> ESPNowConnection  (addTransport, manual)
             ──[MIDITransport*]──> YourTransport     (addTransport, manual)
```

### Creating a custom transport

```cpp
#include "MIDITransport.h"

class MyTransport : public MIDITransport {
public:
    bool begin() { /* init hardware */ return true; }

    void task() override {
        // Read from hardware, then deliver to MIDIHandler:
        if (hasData) dispatchMidiData(midiBytes, 3);
    }

    bool isConnected() const override { return initialized; }

    // Optional: override sendMidiMessage() if your transport supports sending
    bool sendMidiMessage(const uint8_t* data, size_t length) override {
        // Send bytes over your transport
        return true;
    }
};

MyTransport custom;
void setup() {
    custom.begin();
    midiHandler.addTransport(&custom);
    midiHandler.begin();
}
```

---

## ESP-NOW MIDI

ESP-NOW provides ultra-low latency wireless MIDI (~1-5ms) between ESP32 devices. No WiFi router needed — devices communicate directly. Ideal for wireless MIDI on stage.

| Feature | Value |
|---------|-------|
| Latency | ~1-5ms (vs 10-20ms for BLE) |
| Range | ~200m outdoor, ~50m indoor |
| Pairing | Not required (broadcast mode) |
| Max peers | 20 (unicast) / unlimited (broadcast) |
| Bidirectional | Yes |

### Basic usage

```cpp
#include "ESP32_Host_MIDI.h"
#include "ESPNowConnection.h"

ESPNowConnection espNow;

void setup() {
    espNow.begin();                          // Init WiFi + ESP-NOW
    midiHandler.addTransport(&espNow);       // Register with MIDIHandler
    midiHandler.begin();
}

void loop() {
    midiHandler.task();
    // Incoming ESP-NOW MIDI is now parsed like USB/BLE:
    // getActiveNotes(), getAnswer(), sendNoteOn(), etc.
}
```

### Standalone (without MIDIHandler)

```cpp
#include "ESPNowConnection.h"

ESPNowConnection espNow;

void onData(void* ctx, const uint8_t* data, size_t len) {
    Serial.printf("MIDI: %02X %02X %02X\n", data[0], data[1], data[2]);
}

void setup() {
    espNow.begin();
    espNow.setMidiCallback(onData, nullptr);
}

void loop() {
    espNow.task();
    // Send a NoteOn
    uint8_t msg[] = {0x90, 60, 100};
    espNow.sendMidiMessage(msg, 3);
}
```

### Peer management

```cpp
// By default, ESP-NOW broadcasts to all devices on the same channel.
// To target specific devices:
uint8_t peerMAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
espNow.addPeer(peerMAC);

// Get this device's MAC (tell the other device to add it):
uint8_t myMAC[6];
espNow.getLocalMAC(myMAC);
```

---

## Using raw MIDI data (without MIDIHandler)

For custom parsers, MIDI routing, or minimal footprint, use `USBConnection` or `BLEConnection` directly with callbacks:

```cpp
#include "USBConnection.h"
#include "BLEConnection.h"

USBConnection usb;
BLEConnection ble;

void onUsbData(void* ctx, const uint8_t* data, size_t len) {
    // USB-MIDI: 4 bytes per event [CIN, Status, Data1, Data2]
    Serial.printf("USB: %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3]);
}

void onBleData(void* ctx, const uint8_t* data, size_t len) {
    // BLE: raw MIDI bytes (header already stripped)
    Serial.printf("BLE: %02X %02X %02X\n", data[0], data[1], data[2]);
}

void setup() {
    usb.setMidiCallback(onUsbData, nullptr);
    usb.setConnectionCallbacks(onConnect, onDisconnect, nullptr);
    usb.begin();

    ble.setMidiCallback(onBleData, nullptr);
    ble.begin("My Device");
}

void loop() {
    usb.task();
    ble.task();
}
```

---

## Configuration

```cpp
MIDIHandlerConfig config;
config.maxEvents = 30;            // Event queue size (SRAM)
config.chordTimeWindow = 50;      // Chord grouping window (ms). 0 = legacy
config.velocityThreshold = 10;    // Ignore ghost notes below this velocity
config.historyCapacity = 1000;    // PSRAM history buffer. 0 = disabled
config.bleName = "My MIDI Device"; // BLE advertising name

midiHandler.begin(config);
```

| Parameter | Default | Description |
|-----------|---------|-------------|
| `maxEvents` | 20 | Maximum events in the active queue (SRAM) |
| `chordTimeWindow` | 0 | Time window (ms) for chord grouping. 0 = legacy mode |
| `velocityThreshold` | 0 | Minimum velocity to accept NoteOn. 0 = accept all |
| `historyCapacity` | 0 | PSRAM history buffer size. 0 = disabled |
| `bleName` | `"ESP32 MIDI BLE"` | BLE advertising device name |

---

## Architecture

All transports share the same pattern: data arrives in a background task/callback, gets enqueued into a spinlock-protected ring buffer, and is processed on the main loop via `task()`.

```
┌──────────────────────────────┐    ┌──────────────────────────────┐
│       Background Tasks        │    │     Core 1 (main loop)        │
│                               │    │                               │
│  USB: _usbTask (Core 0)      │    │   midiHandler.task()          │
│  • USB host polling           │───>│   • transport[0]->task()      │
│  • Enqueue to ring buffer     │    │   • transport[1]->task()      │
│                               │    │   • transport[N]->task()      │
│  BLE: ESP-IDF BLE task        │    │   • handleMidiMessage()       │
│  • onWrite callback           │───>│   • User logic                │
│  • Enqueue to ring buffer     │    │   • Display rendering         │
│                               │    │                               │
│  ESP-NOW: WiFi task           │    │                               │
│  • _onReceive callback        │───>│                               │
│  • Enqueue to ring buffer     │    │                               │
└──────────────────────────────┘    └──────────────────────────────┘
          ▲                                     │
          │      Ring Buffers (64 msgs each)    │
          └──────── spinlock-safe ──────────────┘
```

### Connection lifecycle (BLE)

- **Advertising** starts automatically in `begin()`.
- When a central (phone/DAW) **connects**, the transport dispatches a connection event.
- When it **disconnects**, active notes are cleared (prevents stuck notes) and advertising restarts automatically.

---

## Music theory with Gingoduino

For chord identification, harmonic field deduction, and progression analysis, use the [Gingoduino](https://github.com/sauloverissimo/gingoduino) library (v0.2.2+) with the optional `GingoAdapter.h`:

```cpp
#include <ESP32_Host_MIDI.h>
#include <GingoAdapter.h>

void loop() {
    midiHandler.task();

    char chordName[16];
    if (GingoAdapter::identifyLastChord(midiHandler, chordName, sizeof(chordName))) {
        Serial.println(chordName);  // "CM", "Am7", "Gdim"
    }
}
```

> See the **T-Display-S3-Gingoduino** example for a complete working sketch.

---

## Examples

| Example | Description |
|---------|-------------|
| **Raw-USB-BLE** | USB and BLE raw access via callbacks without MIDIHandler. Serial output only. |
| **ESP-NOW-MIDI** | Wireless MIDI between two ESP32 devices via ESP-NOW. Broadcast mode. |
| **T-Display-S3** | Note names on ST7789 display using `getAnswer("noteName")`. |
| **T-Display-S3-Queue** | Full event queue and active notes on display. Button to clear. |
| **T-Display-S3-Gingoduino** | Music theory: notes, intervals, chords, harmonic fields on display. |
| **T-Display-S3-Piano** | 25-key piano visualizer + PCM5102A synth + Gingoduino analysis. |
| **T-Display-S3-Piano-Debug** | On-display MIDI monitor for debugging without Serial (USB Host mode). |

---

## File structure

```
src/
  ESP32_Host_MIDI.h       — Main header (includes everything)
  MIDITransport.h         — Abstract transport interface
  MIDIHandlerConfig.h     — Configuration struct
  MIDIHandler.h/.cpp      — MIDI parsing, event queue, chord detection, transport orchestration
  USBConnection.h/.cpp    — USB Host MIDI (ring buffer, Core 0 task)
  BLEConnection.h/.cpp    — BLE MIDI (ring buffer, send/receive, GATT server)
  ESPNowConnection.h/.cpp — ESP-NOW MIDI (ring buffer, broadcast/unicast)
  GingoAdapter.h          — Optional bridge to Gingoduino

examples/
  Raw-USB-BLE/              — Raw MIDI via callbacks (no MIDIHandler)
  ESP-NOW-MIDI/             — Wireless MIDI between ESP32 devices
  T-Display-S3/             — Basic display example
  T-Display-S3-Queue/       — Event queue visualization
  T-Display-S3-Gingoduino/  — Music theory analysis
  T-Display-S3-Piano/       — Piano visualizer + synth
  T-Display-S3-Piano-Debug/ — On-display MIDI debugger
```

---

## Contributing

Contributions, bug reports, and suggestions are welcome! Open an issue or submit a pull request on [GitHub](https://github.com/sauloverissimo/ESP32_Host_MIDI).

## License

MIT License. See [LICENSE](LICENSE.txt).
