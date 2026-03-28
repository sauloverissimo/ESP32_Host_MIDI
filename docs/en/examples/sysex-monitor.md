# 🔧 T-Display-S3 SysEx Monitor

The `T-Display-S3-SysEx` example turns the LilyGO T-Display-S3 into a SysEx monitor with Identity Request sending and real-time MIDI event visualization.

---

## Required Hardware

| Component | Model |
|-----------|-------|
| Board | LilyGO T-Display-S3 |
| Display | ST7789 1.9" 170x320 (built-in) |
| Keyboard/Controller | Any USB MIDI class-compliant device |
| Cable | USB-OTG (micro to USB-A female) |

---

## What the Example Does

- **SysEx Monitor**: displays received SysEx messages with type identification (Identity Request/Reply, Universal RT/NRT, Manufacturer-specific)
- **Hex view**: shows message bytes in hexadecimal, with smart truncation for long messages
- **Identity Request sending**: BTN1 (GPIO0) sends `F0 7E 7F 06 01 F7` to identify the connected device
- **MIDI event log**: shows the latest channel events (NoteOn, NoteOff, CC, etc.)
- **Counters**: queued SysEx messages, MIDI events, active notes

---

## Controls

| Button | GPIO | Function |
|--------|------|----------|
| BTN1 | 0 | Send Identity Request |
| BTN2 | 14 | Clear queues (SysEx + MIDI) |

---

## Arduino IDE Settings

```
Board:           ESP32S3 Dev Module
USB Mode:        USB Host (USB-OTG)
USB CDC on Boot: Enabled
PSRAM:           OPI PSRAM
Flash Size:      16MB (or your board's size)
Partition:       Huge APP (3MB)
```

---

## Source Code

The full example is at [`examples/T-Display-S3-SysEx/`](https://github.com/sauloverissimo/ESP32_Host_MIDI/tree/main/examples/T-Display-S3-SysEx)

The key part is the SysEx configuration:

```cpp
MIDIHandlerConfig config;
config.maxSysExSize = 256;    // max bytes per message
config.maxSysExEvents = 8;    // how many messages in the queue
midiHandler.begin(config);
```

And reading the queue in the loop:

```cpp
const auto& sysexQueue = midiHandler.getSysExQueue();
for (auto it = sysexQueue.rbegin(); it != sysexQueue.rend(); ++it) {
    // it->index, it->timestamp, it->data
}
```

---

## Result

![T-Display-S3 SysEx Monitor](https://github.com/sauloverissimo/ESP32_Host_MIDI/raw/main/examples/T-Display-S3-SysEx/images/sysex.jpeg)

The display shows the header with counters (SysEx, MIDI, active notes), followed by the most recent SysEx messages with type identification, and the latest channel MIDI events at the bottom.
