# Migration Guide: v5.x → v6.0

ESP32_Host_MIDI v5.2 introduces new MIDI spec-compliant fields alongside the existing ones.
In v6.0, the deprecated fields will be removed. This guide helps you migrate.

## Field Mapping

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

- **v5.2**: New fields added. Old fields still work but are deprecated.
- **v6.0**: Deprecated fields removed. Struct becomes POD/trivially-copyable (~32 bytes vs ~160+ bytes).
