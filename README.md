# ESP32_Host_MIDI 🎹📡

![image](https://github.com/user-attachments/assets/bba1c679-6c76-45b7-aa29-a3201a69b36a)

Project developed for the Arduino IDE.

This project provides a complete solution for receiving, interpreting, and displaying MIDI messages via USB and BLE on the ESP32-S2/S3 using devices like the T‑Display S3. The library is modular and can be easily adapted to other hardware by providing a `mapping.h` file with pin definitions for your board.

---

## Compatibility

### ESP32 Family

The library uses **compile-time feature detection** (`#ifdef`) to automatically enable or disable USB and BLE based on the target chip. You only need to select the correct board in the Arduino IDE — the library adapts automatically.

| Board | USB Host | BLE | Dual-Core | PSRAM | MIDIHandler | Status |
|-------|:---:|:---:|:---:|:---:|:---:|---|
| **ESP32-S3** | ✅ | ✅ | ✅ | ✅ (most modules) | ✅ USB + BLE | **Fully supported** (recommended) |
| **ESP32-S2** | ✅ | ❌ | ❌ | depends | ✅ USB only | Supported |
| **ESP32** (classic) | ❌ | ✅ | ✅ | depends | ✅ BLE only | Supported |
| **ESP32-C3** | ❌ | ✅ | ❌ | ❌ | ✅ BLE only | Supported ⚠️ |
| **ESP32-C6** | ❌ | ✅ | ❌ | ❌ | ✅ BLE only | Supported ⚠️ |
| **ESP32-H2** | ❌ | ✅ | ❌ | ❌ | ✅ BLE only | Supported ⚠️ |
| **ESP32-P4** | ✅ | ❌ | ✅ | ✅ | ✅ USB only | Supported |

> ⚠️ **Single-core warning:** On single-core chips (ESP32-S2, ESP32-C3, ESP32-C6, ESP32-H2), USB or BLE processing shares CPU time with your `loop()`. If `loop()` performs heavy operations (display rendering, Serial prints), MIDI events may be delayed or lost. On ESP32-S2 (USB), the library mitigates this with a higher-priority FreeRTOS task. On single-core BLE chips, keep `loop()` lightweight and call `midiHandler.task()` frequently.

### Feature Detection Macros

The library defines these macros automatically in `ESP32_Host_MIDI.h`:

| Macro | Condition | Effect |
|-------|-----------|--------|
| `ESP32_HOST_MIDI_HAS_USB` | ESP32-S2 or ESP32-S3 | Enables USBConnection |
| `ESP32_HOST_MIDI_HAS_BLE` | `CONFIG_BT_ENABLED` | Enables BLEConnection |
| `ESP32_HOST_MIDI_HAS_PSRAM` | `CONFIG_SPIRAM` | Uses PSRAM for history buffer |

You can check these in your sketch:
```cpp
#if ESP32_HOST_MIDI_HAS_USB
  Serial.println("USB Host available");
#endif
#if ESP32_HOST_MIDI_HAS_BLE
  Serial.println("BLE MIDI available");
#endif
```

### Other Platforms

| Board | Compatible? | Reason |
|-------|:---:|---|
| **Arduino Nano ESP32** (ESP32-S3) | ✅ | It's an ESP32-S3 — works natively |
| **Raspberry Pi Pico / Pico 2** (RP2040/RP2350) | ❌ | Different USB Host API (TinyUSB/PIO), no ESP-IDF, no ESP32 BLE stack |
| **Arduino Uno / Mega** (AVR) | ❌ | No USB Host, no BLE, no C++ STL (`<deque>`, `<string>`, `<vector>`) |
| **Arduino Nano 33 BLE** (nRF52840) | ❌ | BLE API differs (ArduinoBLE vs ESP32 BLE), no USB Host |
| **Teensy 4.x** | ❌ | USB Host API differs, no ESP-IDF |
| **Daisy Seed** (STM32H7) | ❌ | Different USB Host API (STM32 HAL), no ESP-IDF |
| **Seeed XIAO ESP32-S3** | ✅ | It's an ESP32-S3 — works natively |
| **Seeed XIAO ESP32-C3** | ✅ | BLE only (no USB Host) |

> **Note:** See [ROADMAP.md](ROADMAP.md) for the plan to support additional platforms in future releases.

---

## Overview

The **ESP32_Host_MIDI** library allows the ESP32 to:
- Act as a USB host for MIDI devices (via the **USBConnection** module),
- Function as a BLE MIDI server (via the **BLEConnection** module),
- Process and interpret MIDI messages (using the **MIDIHandler** module), and
- Display formatted MIDI data on a display (via the **ST7789_Handler** module in examples).

The core header **ESP32_Host_MIDI.h** integrates the USB/BLE connectivity and MIDI handling functionalities.

---

## Architecture: Dual-Core USB Processing

One of the key design decisions in this library is the separation of USB MIDI reception from the main Arduino `loop()`:

```
┌─────────────────────────┐    ┌─────────────────────────┐
│       Core 0            │    │       Core 1             │
│  ┌───────────────────┐  │    │  ┌────────────────────┐  │
│  │   _usbTask()      │  │    │  │   Arduino loop()   │  │
│  │                    │  │    │  │                    │  │
│  │ • USB host polling │  │    │  │ • midiHandler.task │  │
│  │ • Transfer submit  │──┼────┼──│   → processQueue() │  │
│  │ • _onReceive()     │  │    │  │ • Display render   │  │
│  │ • Enqueue to ring  │  │    │  │ • User logic       │  │
│  │   buffer (spinlock)│  │    │  │                    │  │
│  └───────────────────┘  │    │  └────────────────────┘  │
└─────────────────────────┘    └─────────────────────────┘
         ▲                              │
         │      Ring Buffer (64 msgs)   │
         └──────── spinlock-safe ───────┘
```

**Why?** USB MIDI devices can send multiple events in a single USB transfer (up to 16 events in a 64-byte packet). If the main loop is busy (e.g., rendering a display via SPI, which can take 20-50ms), USB polling stalls and MIDI events get lost — especially Note-Off messages during fast chord playing.

**How it works:**
1. `USBConnection::begin()` creates a dedicated FreeRTOS task pinned to **core 0** with priority 5.
2. This task continuously calls `usb_host_lib_handle_events()` and `usb_host_client_handle_events()` in a tight loop, ensuring USB transfers are always serviced promptly.
3. When MIDI data arrives, `_onReceive()` iterates over all 4-byte USB-MIDI packets in the transfer and enqueues them into a **spinlock-protected ring buffer**.
4. On **core 1**, `task()` (called from the Arduino `loop()`) drains the ring buffer and processes each MIDI event via `handleMidiMessage()`.

> On single-core ESP32 variants (ESP32-S2), the dedicated task still helps because its higher priority allows FreeRTOS to preempt the main loop for USB servicing.

---

## File Structure

### Core Library Files (in the `src/` folder)

- **USBConnection.h / USBConnection.cpp**
  Implements the USB host functionality to receive MIDI data from connected MIDI devices.
  USB event handling runs on a dedicated FreeRTOS task (core 0) for reliable reception.
  - **Key Functions:**
    - `begin()`: Initializes the USB host, registers the client, and starts the USB task on core 0.
    - `task()`: Drains the ring buffer and forwards MIDI data to `onMidiDataReceived()`. Call this from `loop()`.
    - `onMidiDataReceived()`: Virtual function (to be overridden) for processing received MIDI messages (4 bytes: CIN + 3 MIDI bytes).

- **BLEConnection.h / BLEConnection.cpp**
  Implements the BLE MIDI server, enabling the ESP32 to receive MIDI messages via Bluetooth Low Energy.
  - **Key Functions:**
    - `begin()`: Initializes the BLE server and starts advertising the MIDI service.
    - `task()`: Processes BLE events (if needed).
    - `setMidiMessageCallback()`: Registers a callback to handle incoming BLE MIDI messages.
    - `onMidiDataReceived()`: Virtual function (to be overridden) for processing BLE MIDI messages.

- **MIDIHandler.h / MIDIHandler.cpp**
  Processes and interprets raw MIDI data (removing USB headers when necessary) and manages MIDI events.
  Uses **MIDI 1.0 standard terminology** for all fields and status types.
  - **Features:**
    - Handles MIDI events: NoteOn, NoteOff, ControlChange, ProgramChange, PitchBend, and ChannelPressure.
    - Converts MIDI note numbers into musical notes (e.g., "C4").
    - Groups simultaneous notes into chords via `chordIndex`.
    - Maintains the state of active notes and an optional history buffer (using PSRAM).
    - Provides utility functions to retrieve formatted MIDI event data.
  - **MIDIEventData struct fields:**
    - `index` — Global event counter
    - `msgIndex` — Links NoteOn/NoteOff pairs
    - `timestamp` — Event time in milliseconds
    - `delay` — Delta time since previous event
    - `channel` — MIDI channel (1-16)
    - `status` — Event type: `"NoteOn"`, `"NoteOff"`, `"ControlChange"`, `"ProgramChange"`, `"PitchBend"`, `"ChannelPressure"`
    - `note` — MIDI note number (or controller number for ControlChange)
    - `noteName` — Musical note name (e.g., "C", "D#")
    - `noteOctave` — Note with octave (e.g., "C4", "D#5")
    - `velocity` — Velocity (or CC value, program number, or pressure)
    - `chordIndex` — Groups simultaneously pressed notes
    - `pitchBend` — 14-bit pitch bend value (0-16383, center = 8192)
  - **Key Functions:**
    - `begin()`: Initializes the MIDI handler and associated USB/BLE connections.
    - `task()`: Processes incoming USB and BLE MIDI events.
    - `handleMidiMessage(data, length)`: Interprets raw MIDI messages.
    - `getChord(chord, queue, fields)`: Retrieves events from a specific chord.
    - `lastChord(queue)`: Returns the highest chord index.
    - `getAnswer(field)`: Returns data from the last chord (e.g., `getAnswer("noteName")`).
    - `clearQueue()`: Clears the event queue and resets all internal state.
    - `enableHistory(capacity)`: Enables a PSRAM history buffer.

- **MIDIHandlerConfig.h**
  Configuration struct for calibrating MIDIHandler behavior (chord detection, velocity filter, queue size, etc.).

- **GingoAdapter.h** *(optional)*
  Bridge header for [Gingoduino](https://github.com/sauloverissimo/gingoduino) integration. Provides functions to convert MIDIHandler chord data into Gingoduino objects for chord identification, harmonic field deduction, and progression analysis. Only include this if you have the Gingoduino library installed.

- **ESP32_Host_MIDI.h**
  The core header that includes the USB/BLE connectivity, configuration, and MIDI handling modules.

### Example Files (in the `examples/` folder)

- **Raw-USB-BLE/** — Demonstrates using USBConnection and BLEConnection directly for raw MIDI byte access, without MIDIHandler. Serial output only, no display required.
- **T-Display-S3/** — Displays the note names from the last MIDI chord on the ST7789 display using `getAnswer("noteName")`.
- **T-Display-S3-Queue/** — Displays the full MIDI event queue and active notes on the display. Includes a hardware button (GPIO 14) to clear the queue at any time. Useful for debugging and detailed event visualization.
- **T-Display-S3-Gingoduino/** — Integrates with the [Gingoduino](https://github.com/sauloverissimo/gingoduino) library (v0.2.2+) on the T-Display S3. Identifies notes (name + frequency), intervals, chords, and deduces harmonic fields — all shown on the ST7789 display and Serial.

---

## Operation

1. **MIDI USB-OTG Reception:**
   When a MIDI device is connected via USB, the **USBConnection** module captures the MIDI data and passes it to **MIDIHandler** for processing.

2. **MIDI BLE Reception:**
   The **BLEConnection** module enables the ESP32 to operate as a BLE MIDI server, receiving MIDI messages from paired Bluetooth devices.

3. **MIDI Message Processing:**
   **MIDIHandler** interprets incoming MIDI messages (NoteOn, NoteOff, ControlChange, ProgramChange, PitchBend, ChannelPressure), converts MIDI note numbers into musical notes, groups simultaneous notes into chords, and optionally stores events in a history buffer.

4. **Display Output:**
   The **ST7789_Handler** module handles the display of formatted MIDI information on the T‑Display S3, ensuring smooth text rendering without flickering.

---

## Configuration

MIDIHandler behavior can be customized using the `MIDIHandlerConfig` struct. All fields have sensible defaults — you only need to change what you want to calibrate:

```cpp
#include <ESP32_Host_MIDI.h>

void setup() {
    MIDIHandlerConfig config;
    config.maxEvents = 30;            // Larger event queue
    config.chordTimeWindow = 50;      // 50ms chord grouping window
    config.velocityThreshold = 10;    // Ignore ghost notes below velocity 10
    config.historyCapacity = 1000;    // Enable PSRAM history
    config.bleName = "My MIDI Device";

    midiHandler.begin(config);
}
```

Calling `midiHandler.begin()` without a config uses all defaults — fully backward compatible.

### Configuration Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `maxEvents` | 20 | Maximum events in the active queue (SRAM) |
| `chordTimeWindow` | 0 | Time window (ms) for chord grouping. 0 = legacy (chord ends only when all notes released) |
| `velocityThreshold` | 0 | Minimum velocity to accept NoteOn. 0 = accept all |
| `historyCapacity` | 0 | PSRAM history buffer size. 0 = disabled |
| `bleName` | `"ESP32 MIDI BLE"` | BLE advertising device name |

---

## Using Raw MIDI Data (Without MIDIHandler)

The **USBConnection** and **BLEConnection** modules can be used independently, without MIDIHandler, when you need direct access to raw MIDI bytes. This is useful for building custom MIDI parsers, MIDI routing, or minimal memory footprint applications.

### USB: Override `onMidiDataReceived()`

```cpp
#include "USBConnection.h"

class MyUSB : public USBConnection {
    void onMidiDataReceived(const uint8_t* data, size_t length) override {
        // data[0] = USB-MIDI CIN byte, data[1..3] = MIDI bytes
        uint8_t status = data[1] & 0xF0;
        uint8_t channel = (data[1] & 0x0F) + 1;
        // ... your processing here
    }
};

MyUSB usb;
void setup() { usb.begin(); }
void loop()  { usb.task(); }
```

### BLE: Use callback or override

```cpp
#include "BLEConnection.h"

BLEConnection ble;

void onMidi(const uint8_t* data, size_t length) {
    // Process raw BLE MIDI data
}

void setup() {
    ble.setMidiMessageCallback(onMidi);
    ble.begin("My Device");
}
void loop() { ble.task(); }
```

See the **Raw-USB-BLE** example for a complete working sketch.

---

## Music Theory with Gingoduino

ESP32_Host_MIDI groups simultaneous notes into chords via `chordIndex`, but does not perform music theory analysis. For chord identification, harmonic field deduction, and progression analysis, use the [Gingoduino](https://github.com/sauloverissimo/gingoduino) library (v0.2.2+) together with the optional **GingoAdapter.h** bridge header.

### Using GingoAdapter (recommended)

```cpp
#include <ESP32_Host_MIDI.h>
#include <GingoAdapter.h>

using namespace gingoduino;

void loop() {
    midiHandler.task();

    // Identify chords
    char chordName[16];
    if (GingoAdapter::identifyLastChord(midiHandler, chordName, sizeof(chordName))) {
        Serial.println(chordName);  // "CM", "Am7", "Gdim"
    }

    // Deduce harmonic field from all chords in the queue
    FieldMatch fields[3];
    uint8_t n = GingoAdapter::deduceFieldFromQueue(midiHandler, fields, 3);
    if (n > 0) {
        Serial.printf("Field: %s (matched %d/%d)\n",
                      fields[0].tonicName, fields[0].matched, fields[0].total);
    }
}
```

### GingoAdapter Functions

**Note Conversion:**

| Function | Description |
|----------|-------------|
| `midiToGingoNotes(midiNotes, count, output)` | Converts MIDI note numbers to sorted `GingoNote` array. |
| `activeNotesToGingo(handler, output)` | Converts currently active notes to `GingoNote` array. |

**Chord Identification:**

| Function | Description |
|----------|-------------|
| `identifyLastChord(handler, output, maxLen)` | Identifies the last chord in the queue. |
| `identifyChord(handler, chordIndex, output, maxLen)` | Identifies a specific chord by index. |

**Harmonic Field Deduction** (requires Gingoduino Tier 2+):

| Function | Description |
|----------|-------------|
| `deduceField(chordNames, count, output, maxResults)` | Deduces harmonic fields from chord name strings. |
| `deduceFieldFromQueue(handler, output, maxResults)` | Scans the MIDIHandler queue, identifies chords, and deduces the harmonic field. |

**Progression Analysis** (requires Gingoduino Tier 3):

| Function | Description |
|----------|-------------|
| `identifyProgression(tonic, scale, branches, count, result)` | Identifies the best progression match. |
| `predictNext(tonic, scale, branches, count, output, maxResults)` | Predicts next branches from a partial sequence. |

> See the **T-Display-S3-Gingoduino** example for a complete working sketch.

---

## Customization

The library is designed to be modular:
- Each example provides its own `mapping.h` with pin assignments. Create or modify this file to match your board.
- The core modules **USBConnection**, **BLEConnection**, and **MIDIHandler** can be extended or replaced to suit specific application requirements.
- The display handler (`ST7789_Handler`) lives in the examples, not in the core library, so you can replace it with any display driver.

---

## Getting Started

1. **Install Dependencies:**
   - Arduino IDE (1.8.19+ or 2.x).
   - ESP32 Arduino Core 2.0.0+ (includes USB Host and BLE support).
   - [LovyanGFX](https://github.com/lovyan03/LovyanGFX) library (0.4.x+) for display management (T-Display S3 examples).
   - [Gingoduino](https://github.com/sauloverissimo/gingoduino) library (v0.2.2+) — *optional*, only needed for the T-Display-S3-Gingoduino example and GingoAdapter.h.
   - BLE libraries are included in the ESP32 Arduino Core.

2. **Load the Example:**
   Open one of the example sketches from `examples/` in the Arduino IDE, adjust the pin configuration in the example's `mapping.h` if necessary, and upload it to your ESP32-S3 board.
   - Select board: **ESP32S3 Dev Module** (or your specific board).
   - Set **USB Mode** to **USB-OTG (TinyUSB)** in the board settings.

3. **Connect a MIDI Device:**
   Use a USB MIDI device or pair a BLE MIDI device to test MIDI message reception and display.

---

## Contributing

Contributions, bug reports, and suggestions are welcome!
Feel free to open an issue or submit a pull request on GitHub.

---

## License

This project is released under the MIT License. See the [LICENSE](LICENSE.txt) file for details.
