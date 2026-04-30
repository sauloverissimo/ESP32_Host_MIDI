# Migration Guide: v5.x → v6.0

v6.0 contains two breaking changes that require code updates in user
sketches. Both are independent of each other; you may need one or both
depending on what your firmware uses.

1. **Transport architecture**: `MIDIHandler` no longer owns built-in USB
   Host and BLE transports. User code instantiates each transport it
   uses and registers it with `addTransport()`.
2. **`MIDIEventData` field cleanup**: the deprecated string / 1-indexed
   fields added before v5.2 are removed; only the spec-compliant
   replacements (`statusCode`, `channel0`, etc.) remain.

The two are documented in turn below.

---

## Part 1: Transport architecture (new in v6.0)

### What changed

In v5.x `MIDIHandler` owned `USBConnection` and `BLEConnection` as
private members and auto-registered them in `begin()`. The umbrella
header `ESP32_Host_MIDI.h` auto-included both transport headers based
on chip capability flags. Result: every consumer of the library paid
the compile cost of USB Host and BLE even when neither was used, and
the handler API was tied to those two transports specifically.

In v6.0:

- `MIDIHandler` has no built-in transport members.
- `ESP32_Host_MIDI.h` no longer auto-includes `USBConnection.h` or
  `BLEConnection.h`. Each transport lives behind its own header and the
  application includes only what it uses.
- `MIDIHandler::isBleConnected()` is removed. Query the
  `BLEConnection` instance directly via its own `isConnected()`.
- `MIDIHandlerConfig::bleName` is no longer used by `MIDIHandler` (it
  used to feed the auto-registered BLE transport's `begin()`). Pass the
  device name explicitly to `BLEConnection::begin()`.

### Migration pattern

For every sketch that relied on `midiHandler.begin()` auto-starting
USB Host or BLE, add explicit transport instantiation:

```cpp
// v5.x: relied on auto-start
#include <ESP32_Host_MIDI.h>

void setup() {
    MIDIHandlerConfig cfg;
    cfg.bleName = "My Device";
    midiHandler.begin(cfg);   // auto-instantiated USB Host + BLE
}
```

```cpp
// v6.0: explicit
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>     // include each transport you actually use
#include <BLEConnection.h>

USBConnection usbHost;          // declare globally (TinyUSB needs it
BLEConnection bleHost;          // before USB.begin())

void setup() {
    MIDIHandlerConfig cfg;
    midiHandler.addTransport(&usbHost);
    midiHandler.addTransport(&bleHost);
    usbHost.begin();
    bleHost.begin("My Device");
    midiHandler.begin(cfg);
}
```

### Available transport headers

```
#include <USBConnection.h>           // USB Host MIDI 1.0
#include <USBMIDI2Connection.h>      // USB Host MIDI 2.0 (UMP native)
#include <USBDeviceConnection.h>     // USB Device MIDI 1.0
#include <BLEConnection.h>           // BLE peripheral
#include <UARTConnection.h>          // DIN-5 / TRS UART
#include <ESPNowConnection.h>        // ESP-NOW peer-to-peer wireless
#include <RTPMIDIConnection.h>       // RTP-MIDI / Apple MIDI over WiFi
#include <EthernetMIDIConnection.h>  // Apple MIDI over W5500 / P4 EMAC
#include <OSCConnection.h>           // OSC bidirectional bridge
#include <MIDI2UDPConnection.h>      // raw UMP over UDP
```

### `isBleConnected` replacement

```cpp
// v5.x
if (midiHandler.isBleConnected()) { ... }
```

```cpp
// v6.0: query the BLE instance directly
if (bleHost.isConnected()) { ... }
```

### Why the change

Coupling forced compilation of USB Host and BLE drivers into firmware
that did not use them. That made trivial Arduino IDE / PlatformIO
toolchain quirks (BLE std::string vs Arduino String, the BLE2902 CCCD
descriptor's deprecation, the USB-MIDI 2.0 alt-switch race) cascade
into build failures or runtime regressions for users who never
touched the relevant code paths. The decoupling makes each transport
pay its own cost only when explicitly used, and the library API stops
pretending those two transports are special among the nine available.

---

## Part 2: `MIDIEventData` field cleanup

### Field Mapping

| v5.x (deprecated in v5.2) | v5.2+ / v6.0 replacement | Notes |
|---|---|---|
| `ev.status == "NoteOn"` | `ev.statusCode == MIDI_NOTE_ON` | Enum comparison, zero-cost |
| `ev.status == "NoteOff"` | `ev.statusCode == MIDI_NOTE_OFF` | |
| `ev.status == "ControlChange"` | `ev.statusCode == MIDI_CONTROL_CHANGE` | |
| `ev.status == "PitchBend"` | `ev.statusCode == MIDI_PITCH_BEND` | |
| `ev.status == "ProgramChange"` | `ev.statusCode == MIDI_PROGRAM_CHANGE` | |
| `ev.status == "ChannelPressure"` | `ev.statusCode == MIDI_CHANNEL_PRESSURE` | |
| `ev.channel` (1-16) | `ev.channel0` (0-15) | MIDI spec convention; display as `ev.channel0 + 1` |
| `ev.note` | `ev.noteNumber` | Same value (0-127) |
| `ev.velocity` (7-bit) | `ev.velocity7` (7-bit) or `ev.velocity16` (16-bit) | `velocity16` uses MIDI 2.0 scaling |
| `ev.pitchBend` (14-bit) | `ev.pitchBend14` (14-bit) or `ev.pitchBend32` (32-bit) | `pitchBend32` uses MIDI 2.0 scaling |
| `ev.status.c_str()` (display) | `MIDIHandler::statusName(ev.statusCode)` | Returns `const char*`, zero allocation |
| `ev.noteName` (std::string) | `MIDIHandler::noteName(ev.noteNumber)` | Returns `const char*`, zero allocation |
| `ev.noteOctave` (std::string) | `MIDIHandler::noteWithOctave(ev.noteNumber, buf, sizeof(buf))` | Caller provides `char buf[8]` |

## Code Examples

### Before (v5.x)

```cpp
if (ev.status == "NoteOn") {
    Serial.printf("Ch %d Note %s Vel %d\n",
        ev.channel, ev.noteOctave.c_str(), ev.velocity);
}
```

### After (v5.2+ / v6.0)

```cpp
char noteBuf[8];
if (ev.statusCode == MIDI_NOTE_ON) {
    Serial.printf("Ch %d Note %s Vel %d\n",
        ev.channel0 + 1,
        MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
        ev.velocity7);
}
```

### Pitch Bend

```cpp
// Before:
int bend = ev.pitchBend - 8192;

// After:
int bend = (int)ev.pitchBend14 - 8192;
// Or use 32-bit MIDI 2.0 resolution:
// uint32_t bend32 = ev.pitchBend32;  // center = 0x80000000
```

### Building raw MIDI bytes

```cpp
// Before:
data[0] = 0x90 | ((ev.channel - 1) & 0x0F);

// After:
data[0] = 0x90 | (ev.channel0 & 0x0F);
```

## MIDIStatus Enum Values

```cpp
enum MIDIStatus : uint8_t {
    MIDI_NOTE_OFF          = 0x80,
    MIDI_NOTE_ON           = 0x90,
    MIDI_POLY_PRESSURE     = 0xA0,
    MIDI_CONTROL_CHANGE    = 0xB0,
    MIDI_PROGRAM_CHANGE    = 0xC0,
    MIDI_CHANNEL_PRESSURE  = 0xD0,
    MIDI_PITCH_BEND        = 0xE0,
};
```

Values match the upper nibble of MIDI 1.0 status bytes — no conversion needed.

## Fields NOT changing

These fields remain the same in v6.0:

- `ev.index`, `ev.msgIndex`, `ev.timestamp`, `ev.delay`, `ev.chordIndex`

## Timeline

- **v5.2**: New `MIDIEventData` fields added. Old fields still work but are deprecated.
- **v5.2.x**: Bug fixes; transport architecture unchanged.
- **v6.0**: Deprecated `MIDIEventData` fields removed (`MIDIEventData` becomes POD / trivially-copyable, ~32 bytes vs ~160+ bytes). `MIDIHandler` decoupled from built-in transports (Part 1 above). Bug fixes on top of v5.2.x: BLE compat with arduino-esp32 v2.x and v3.x; explicit SET_INTERFACE in `USBMIDI2Connection`.
