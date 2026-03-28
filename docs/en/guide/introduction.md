# Introduction

**ESP32_Host_MIDI** is an open-source Arduino library that turns the ESP32 into a universal MIDI hub with support for **9 simultaneous transports**, all operating through the same clean event-based API.

---

## What the Library Does

The core idea is simple: no matter where MIDI comes from -- USB, Bluetooth, WiFi, serial cable, radio -- it always arrives in the same event queue (`getQueue()`), with the same format (`MIDIEventData`), ready to process.

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    ev.statusCode;  // MIDIStatus enum: MIDI_NOTE_ON, MIDI_NOTE_OFF, MIDI_CONTROL_CHANGE...
    ev.channel0;    // 0-15 (add +1 to display 1-16)
    ev.noteNumber;  // MIDI number (0-127)
    ev.velocity7;   // 0-127
    ev.velocity16;  // 0-65535 (MIDI 2.0)
    ev.pitchBend14; // 0-16383 (MIDI 1.0, center = 8192)
    ev.pitchBend32; // 0-4294967295 (MIDI 2.0)
    ev.timestamp;   // millis() on arrival
    ev.chordIndex;  // groups simultaneous notes
    // Static helpers: MIDIHandler::statusName(), noteWithOctave(), noteName()
}
```

At the same time, `midiHandler.sendNoteOn()` and other send methods transmit to **all** active transports simultaneously. An event arriving via USB can immediately go out through BLE, DIN-5, and WiFi -- with no extra code.

---

## The 9 Transports

```mermaid
mindmap
  root((ESP32\nHost MIDI))
    USB Host
      ESP32-S3 / S2 / P4
      Class-compliant keyboards
      Latency < 1 ms
    USB MIDI 2.0
      ESP32-S3 / S2 / P4
      USBMIDI2Connection / UMP
      Native Protocol Negotiation
    BLE MIDI
      Any ESP32 with BT
      iOS - macOS - Android
      Latency 3-15 ms
    USB Device
      ESP32-S3 / S2 / P4
      Appears as USB interface
      Latency < 1 ms
    ESP-NOW
      Any ESP32
      P2P mesh without router
      Latency 1-5 ms
    RTP-MIDI WiFi
      AppleMIDI / RFC 6295
      macOS - iOS - Logic Pro
      Latency 5-20 ms
    Ethernet MIDI
      W5500 SPI or P4 native
      Ideal for studios
      Latency 2-10 ms
    OSC
      Bidirectional WiFi UDP
      Max/MSP - PD - SC
      Latency 5-15 ms
    UART / DIN-5
      Any ESP32
      Vintage synthesizers
      Latency < 1 ms
```

---

## Software Architecture

### FreeRTOS Core Separation

The ESP32 has two cores. The library uses this separation to ensure low latency:

```mermaid
graph TD
    subgraph CORE0["Core 0 -- Drivers and Stack"]
        USB_TASK["USB Host Task\n(USBConnection)"]
        BLE_STACK["BLE Stack\n(BLEConnection)"]
        WIFI["WiFi / Ethernet Stack\n(RTP-MIDI, OSC, ESP-NOW)"]
    end

    subgraph CORE1["Core 1 -- Your Code"]
        LOOP["loop()"]
        TASK["midiHandler.task()"]
        USER["Your code\n(display, synth, etc.)"]
    end

    subgraph BUFFERS["Ring Buffers (thread-safe)"]
        RB1["portMUX spinlock\nUSB buffer"]
        RB2["portMUX spinlock\nBLE buffer"]
        RB3["portMUX spinlock\nWiFi buffers"]
    end

    USB_TASK --> RB1
    BLE_STACK --> RB2
    WIFI --> RB3
    RB1 --> TASK
    RB2 --> TASK
    RB3 --> TASK
    TASK --> LOOP
    LOOP --> USER

    style CORE0 fill:#1A237E,color:#fff,stroke:#283593
    style CORE1 fill:#1B5E20,color:#fff,stroke:#2E7D32
    style BUFFERS fill:#37474F,color:#fff,stroke:#546E7A
```

### Flow of a NoteOn Event

```mermaid
sequenceDiagram
    participant USB as USB Keyboard
    participant DRIVER as USB Host Driver
    participant BUF as Ring Buffer
    participant HANDLER as MIDIHandler
    participant USER as Your Code

    USB->>DRIVER: USB-MIDI packet (4 bytes)
    DRIVER->>BUF: Store with portMUX (Core 0)
    loop Each loop()
        USER->>HANDLER: midiHandler.task()
        HANDLER->>BUF: Read pending messages
        BUF->>HANDLER: [0x09, 0x90, 0x3C, 0x64]
        HANDLER->>HANDLER: Parse into MIDIEventData
        Note over HANDLER: statusCode=MIDI_NOTE_ON<br/>noteNumber=60<br/>velocity7=100
        HANDLER->>USER: getQueue() returns event
    end
```

---

## Library Layers

| Layer | File | Responsibility |
|-------|------|---------------|
| Feature detection | `ESP32_Host_MIDI.h` | Detects USB, BLE, PSRAM by chip |
| Abstract transport | `MIDITransport.h` | Common interface for all transports |
| Central processor | `MIDIHandler.h/.cpp` | Queue, chords, active notes, sending |
| Configuration | `MIDIHandlerConfig.h` | Handler configuration struct |
| Built-in transports | `USBConnection`, `BLEConnection`, `ESPNowConnection` | Automatically registered |
| External transports | `UART`, `RTP-MIDI`, `Ethernet`, `OSC`, `USBDevice` | Manually included in sketch |
| Theory integration | `GingoAdapter.h` | Bridge with Gingoduino |

---

## Typical Use Cases

### Stage MIDI Hub

```
USB Keyboard ------------------------------------------------+
iPhone BLE --------------------------------------------------+
ESP-NOW (pedals) --------------------------------------------+---> MIDIHandler ---> USB Device -> FOH Computer
                                                              |              ---> DIN-5 -> Effects rack
                                                              |              ---> ESP-NOW -> Other performers
```

### Studio Interface

```
macOS (RTP-MIDI via WiFi) -----------------------------------+
DIN-5 Synthesizer -------------------------------------------+---> MIDIHandler ---> USB Device -> DAW
iPad (BLE MIDI) ---------------------------------------------+              ---> DIN-5 (THRU) -> Other synths
```

---

## Next Steps

- [Installation ->](installation.md) -- install via Arduino IDE or PlatformIO
- [Getting Started ->](getting-started.md) -- first sketch up and running in 5 minutes
- [Configuration ->](configuration.md) -- `MIDIHandlerConfig` and advanced options
