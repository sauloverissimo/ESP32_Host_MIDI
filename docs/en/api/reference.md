# API Reference

Complete reference of all classes, structures, and methods in the ESP32_Host_MIDI library.

---

## MIDIStatus

Enum with the actual MIDI protocol status bytes:

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

---

## MIDIEventData

Structure representing a parsed MIDI event. Returned by `getQueue()`.

```cpp
struct MIDIEventData {
    int index;                // Global counter (unique, increasing)
    int msgIndex;             // Links NoteOn <-> NoteOff pairs
    unsigned long timestamp;  // millis() at the time of the event
    unsigned long delay;      // Delta t in ms since the previous event

    // v5.2+ -- MIDI spec compliant
    MIDIStatus statusCode;    // MIDI_NOTE_ON | MIDI_NOTE_OFF | MIDI_CONTROL_CHANGE | ...
    uint8_t channel0;         // MIDI channel: 0-15 (MIDI spec)
    uint8_t noteNumber;       // MIDI number (0-127)
    uint16_t velocity16;      // 16-bit velocity (MIDI 2.0, scaled via MIDI2Scaler)
    uint8_t velocity7;        // 7-bit velocity (original MIDI 1.0)
    uint32_t pitchBend32;     // 32-bit pitch bend (MIDI 2.0, center = 0x80000000)
    uint16_t pitchBend14;     // 14-bit pitch bend (original, 0-16383, center = 8192)
    int chordIndex;           // Chord grouping index

    // Deprecated (v5.2) -- will be removed in v6.0
    int channel;              // 1-16 (use channel0)
    std::string status;       // "NoteOn" etc (use statusCode)
    int note;                 // 0-127 (use noteNumber)
    std::string noteName;     // (use MIDIHandler::noteName())
    std::string noteOctave;   // (use MIDIHandler::noteWithOctave())
    int velocity;             // 0-127 (use velocity7 or velocity16)
    int pitchBend;            // 0-16383 (use pitchBend14 or pitchBend32)
};
```

### Field mapping by message type

| `statusCode` | `noteNumber` | `velocity7` | `pitchBend14` |
|---------|--------|-----------|------------|
| `MIDI_NOTE_ON` | MIDI note (0-127) | Velocity (0-127) | 0 |
| `MIDI_NOTE_OFF` | MIDI note (0-127) | Release velocity | 0 |
| `MIDI_CONTROL_CHANGE` | CC number (0-127) | CC value (0-127) | 0 |
| `MIDI_PROGRAM_CHANGE` | Program (0-127) | 0 | 0 |
| `MIDI_PITCH_BEND` | 0 | 0 | 0-16383 (center=8192) |
| `MIDI_CHANNEL_PRESSURE` | 0 | Pressure (0-127) | 0 |

---

## MIDIHandlerConfig

Configuration struct passed to `midiHandler.begin(cfg)`.

```cpp
struct MIDIHandlerConfig {
    int maxEvents = 20;
    // Maximum capacity of the event queue. Oldest events are discarded
    // when the queue reaches this limit.

    unsigned long chordTimeWindow = 0;
    // Time window (ms) to group simultaneous notes under the same chordIndex.
    // 0 = new chord only when ALL notes are released.
    // 30-80 ms = ideal for physical keyboards.

    int velocityThreshold = 0;
    // Filters NoteOn with velocity < velocityThreshold. 0 = disabled.

    int historyCapacity = 0;
    // Capacity of the circular history buffer (in PSRAM if available).
    // 0 = disabled.

    int maxSysExSize = 512;
    // Maximum size of a SysEx message (bytes, including F0 and F7).
    // Larger messages are truncated. 0 = disables SysEx.

    int maxSysExEvents = 8;
    // SysEx queue capacity. Oldest messages are discarded.

    const char* bleName = "ESP32 MIDI BLE";
    // Name advertised by the BLE MIDI peripheral.
};
```

---

## MIDIHandler

Global singleton: `extern MIDIHandler midiHandler;`

### Setup

```cpp
void begin();
// Initializes with default configuration.
// Automatically registers: USBConnection (if S2/S3/P4), BLEConnection (if BT enabled)

void begin(const MIDIHandlerConfig& config);
// Initializes with custom configuration.

void addTransport(MIDITransport* transport);
// Registers an external transport (up to 4 additional external transports).
// Must be called BEFORE begin().

void setQueueLimit(int maxEvents);
// Changes the queue capacity after begin().

void enableHistory(int capacity);
// Enables the circular history buffer (uses PSRAM if available, falls back to heap).
// Can be called after begin().
```

### Loop

```cpp
void task();
// Call in every loop(). Drains ring buffers from all transports,
// parses messages, updates the queue, active notes, and chords.
```

### Receiving -- Event Queue

```cpp
const std::deque<MIDIEventData>& getQueue() const;
// Returns the event queue since the last call to task().
// Iterate with range-for: for (const auto& ev : midiHandler.getQueue()) {}

void clearQueue();
// Empties the queue immediately.
```

### Receiving -- Active Notes

```cpp
std::string getActiveNotes() const;
// Returns a formatted string: "{C4, E4, G4}"

std::string getActiveNotesString() const;
// Alias for getActiveNotes()

std::vector<std::string> getActiveNotesVector() const;
// Returns a vector of strings: ["C4", "E4", "G4"]

size_t getActiveNotesCount() const;
// Number of currently held notes.

void fillActiveNotes(bool out[128]) const;
// Fills array[128] -- out[note] = true if the note is active.

void clearActiveNotesNow();
// Resets the active notes map (useful after reconnection or reset).
```

### Receiving -- Chords

```cpp
int lastChord(const std::deque<MIDIEventData>& queue) const;
// Returns the most recent chordIndex in the queue. -1 if empty.

std::vector<std::string> getChord(
    int chord,
    const std::deque<MIDIEventData>& queue,
    const std::vector<std::string>& fields = {"all"},
    bool includeLabels = false
) const;
// Returns field value(s) for all notes with chordIndex == chord.
// fields: {"noteOctave"} | {"noteName"} | {"velocity"} | {"all"} | combinations
// includeLabels: prefix each value with "field:value"

std::vector<std::string> getAnswer(
    const std::string& field = "all",
    bool includeLabels = false
) const;
// Shortcut for lastChord + getChord with a single field.

std::vector<std::string> getAnswer(
    const std::vector<std::string>& fields,
    bool includeLabels = false
) const;
// Shortcut for lastChord + getChord with multiple fields.
```

### Static Helpers

```cpp
static const char* MIDIHandler::noteName(uint8_t noteNumber);
// Returns note name: "C", "C#", "D", ... (string literal, zero allocation)

static int MIDIHandler::noteOctave(uint8_t noteNumber);
// Returns octave: -1 to 9

static const char* MIDIHandler::noteWithOctave(uint8_t noteNumber, char* buf, size_t bufLen);
// Writes "C4", "D#5" etc into buf (caller's buffer). Returns buf.

static const char* MIDIHandler::statusName(MIDIStatus code);
// Returns status name: "NoteOn", "NoteOff", "ControlChange", etc.
```

### Receiving -- SysEx

```cpp
const std::deque<MIDISysExEvent>& getSysExQueue() const;
// Returns the SysEx queue. Separate from the normal event queue.
// Each MIDISysExEvent contains: index, timestamp, data (vector<uint8_t>).

void clearSysExQueue();
// Empties the SysEx queue.

bool sendSysEx(const uint8_t* data, size_t length);
// Sends SysEx to all transports. Must start with 0xF0 and end with 0xF7.
// Returns false if the message is invalid.

typedef void (*SysExCallback)(const uint8_t* data, size_t length);
void setSysExCallback(SysExCallback cb);
// Optional callback invoked immediately when a complete SysEx message is received.
// Use nullptr to disable.
```

### MIDISysExEvent

```cpp
struct MIDISysExEvent {
    int index;                      // Global increasing counter
    unsigned long timestamp;        // millis() at the time of reception
    std::vector<uint8_t> data;      // Complete message (F0 ... F7)
};
```

---

### Sending MIDI

> **Note:** In v5.2, the send API uses channel 1-16. In v6.0, it will be migrated to 0-15 (MIDI spec).

All send methods transmit to **all transports** that support sending:

```cpp
bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
// channel: 1-16 | note: 0-127 | velocity: 0-127
// Returns true if at least one transport sent successfully.

bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
// velocity is typically 0 for NoteOff.

bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value);
// controller: 0-127 (CC#7=volume, CC#11=expression, CC#64=sustain, etc.)
// value: 0-127

bool sendProgramChange(uint8_t channel, uint8_t program);
// program: 0-127

bool sendPitchBend(uint8_t channel, int value);
// value: -8192 to +8191 (internally converted to 0-16383)

bool sendRaw(const uint8_t* data, size_t length);
// Sends raw MIDI bytes to all transports.

bool sendBleRaw(const uint8_t* data, size_t length);
// Alias for sendRaw() (backward compatibility).
```

### BLE Status

```cpp
#if ESP32_HOST_MIDI_HAS_BLE
bool isBleConnected() const;
// Returns true if a BLE MIDI device is connected.
#endif
```

### Debug Callback

```cpp
typedef void (*RawMidiCallback)(const uint8_t* raw, size_t rawLen,
                                 const uint8_t* midi3);
void setRawMidiCallback(RawMidiCallback cb);
// cb is called with raw MIDI bytes BEFORE parsing.
// raw = complete USB-MIDI payload; midi3 = the 3 MIDI bytes.
// Use nullptr to disable.
```

---

## MIDITransport

Abstract interface implemented by all transports.

```cpp
class MIDITransport {
public:
    virtual ~MIDITransport() = default;

    // Required -- implement in subclasses:
    virtual void task() = 0;                // Called every loop() by MIDIHandler
    virtual bool isConnected() const = 0;   // Current connection status

    // Optional -- implement in subclasses:
    virtual bool sendMidiMessage(const uint8_t* data, size_t length);
    // Returns true if sent. Default: return false.

    // Callback registration (called by MIDIHandler):
    void setMidiCallback(MidiDataCallback cb, void* ctx);
    void setSysExCallback(SysExDataCallback cb, void* ctx);
    void setConnectionCallbacks(ConnectionCallback onConnected,
                                ConnectionCallback onDisconnected,
                                void* ctx);

    // UMP (native MIDI 2.0)
    void setUMPCallback(UMPDataCallback cb, void* ctx);

protected:
    // Call from subclasses to inject data into MIDIHandler:
    void dispatchMidiData(const uint8_t* data, size_t len);
    void dispatchSysExData(const uint8_t* data, size_t len);
    void dispatchUMPData(const uint32_t* words, uint8_t count);  // inject UMP words
    void dispatchConnected();
    void dispatchDisconnected();
};
```

---

## USBConnection

USB Host transport. Automatically included on S2/S3/P4 chips.

```cpp
// Internal use -- do not instantiate directly.
// Configuration: Tools > USB Mode -> "USB Host"
```

---

## USBMIDI2Connection

USB Host with native MIDI 2.0/UMP support. Extends `USBConnection`.

```cpp
#include "src/USBMIDI2Connection.h"

USBMIDI2Connection usb;

// Callbacks
usb.setUMPCallback(onUMP, nullptr);   // Native MIDI 2.0 (UMP words)
usb.setMidiCallback(onMidi, nullptr); // MIDI 1.0 fallback

// Query capabilities after negotiation
usb.isMIDI2();        // true if device negotiated MIDI 2.0
usb.isNegotiated();   // true if Protocol Negotiation completed
usb.getEndpointInfo(); // UMP version, function blocks, protocol
usb.getFunctionBlocks(); // Function Block Info array
usb.getGTBlocks();       // Group Terminal Block array
usb.getEndpointName();   // endpoint name (string from device)
usb.sendUMPMessage(words, count); // send UMP words via OUT endpoint
```

---

## BLEConnection

BLE MIDI transport. Automatically included if `CONFIG_BT_ENABLED`.

```cpp
// Internal use -- do not instantiate directly.
// Name configured via MIDIHandlerConfig::bleName
```

---

## ESPNowConnection

```cpp
#include "src/ESPNowConnection.h"

ESPNowConnection espNow;
espNow.begin(int channel = 0);     // 0 = use current WiFi channel
espNow.addPeer(const uint8_t mac[6]);  // Add peer for unicast
// isConnected() always returns true
```

---

## UARTConnection

```cpp
#include "src/UARTConnection.h"

UARTConnection uart;
uart.begin(HardwareSerial& serial, int rxPin, int txPin);
// serial: Serial1, Serial2, etc.
// rxPin, txPin: GPIOs for RX and TX
// isConnected() always returns true
```

---

## RTPMIDIConnection

```cpp
#include "src/RTPMIDIConnection.h"  // Requires AppleMIDI-Library v3.x

RTPMIDIConnection rtpMIDI;
rtpMIDI.begin(const char* sessionName = "ESP32 MIDI");
// WiFi must be connected BEFORE calling begin()
// isConnected() returns true when an active AppleMIDI session exists
```

---

## EthernetMIDIConnection

```cpp
#include "src/EthernetMIDIConnection.h"  // Requires AppleMIDI + Ethernet

EthernetMIDIConnection ethMIDI;
ethMIDI.begin(const uint8_t mac[6]);                          // DHCP
ethMIDI.begin(const uint8_t mac[6], IPAddress staticIP);      // Static IP
ethMIDI.begin(const uint8_t mac[6], IPAddress ip, int csPin); // Custom CS pin
```

---

## OSCConnection

```cpp
#include "src/OSCConnection.h"  // Requires CNMAT/OSC

OSCConnection osc;
osc.begin(
    int localPort,              // Local UDP port (e.g. 8000)
    IPAddress remoteIP,         // Destination IP (e.g. computer with Max/MSP)
    int remotePort              // Destination UDP port (e.g. 9000)
);
```

Address map:
```
/midi/noteon    channel note velocity
/midi/noteoff   channel note velocity
/midi/cc        channel controller value
/midi/pc        channel program
/midi/pitchbend channel bend
/midi/aftertouch channel pressure
```

---

## USBDeviceConnection

```cpp
#include "src/USBDeviceConnection.h"
// Configuration: Tools > USB Mode -> "USB-OTG (TinyUSB)"

USBDeviceConnection usbDev(const char* deviceName = "ESP32 MIDI");
usbDev.begin();
// Must be called BEFORE midiHandler.begin()
// isConnected() returns true when the USB host is connected
```

---

## MIDI2Support -- Types and Utilities

```cpp
#include "src/MIDI2Support.h"

// Scaling
uint16_t MIDI2Scaler::scale7to16(uint8_t v7);
uint32_t MIDI2Scaler::scale7to32(uint8_t v7);
uint32_t MIDI2Scaler::scale14to32(uint16_t v14);
uint8_t  MIDI2Scaler::scale16to7(uint16_t v16);
uint8_t  MIDI2Scaler::scale32to7(uint32_t v32);
uint16_t MIDI2Scaler::scale32to14(uint32_t v32);

// UMP Builder
UMPWord64 UMPBuilder::noteOn(uint8_t group, uint8_t channel,
                              uint8_t note, uint16_t velocity16);
UMPWord64 UMPBuilder::noteOff(uint8_t group, uint8_t channel,
                               uint8_t note, uint16_t velocity16);
UMPWord64 UMPBuilder::controlChange(uint8_t group, uint8_t channel,
                                     uint8_t index, uint32_t value32);
UMPWord64 UMPBuilder::pitchBend(uint8_t group, uint8_t channel,
                                 uint32_t value32);

// UMP Parser
UMPResult UMPParser::parseMIDI2(UMPWord64 pkt);
```

---

## GingoAdapter

```cpp
#include "src/GingoAdapter.h"  // Requires Gingoduino >= v0.2.2

// Identify the most recent chord name
bool GingoAdapter::identifyLastChord(
    MIDIHandler& handler,
    char* outName,
    size_t nameSize
);

// Convert MIDI notes to GingoNote
uint8_t GingoAdapter::midiToGingoNotes(
    const uint8_t* midiNotes,
    uint8_t count,
    GingoNote* outNotes
);

// Harmonic field (requires GINGODUINO_HAS_FIELD)
#if defined(GINGODUINO_HAS_FIELD)
uint8_t GingoAdapter::deduceFieldFromQueue(
    MIDIHandler& handler,
    FieldMatch* outFields,
    uint8_t maxFields
);
#endif

// Progression (requires GINGODUINO_HAS_PROGRESSION)
#if defined(GINGODUINO_HAS_PROGRESSION)
bool GingoAdapter::identifyProgression(
    const char* root,
    ScaleType scale,
    const char** branches,
    uint8_t branchCount,
    ProgressionMatch* out
);
#endif
```

---

## Feature Detection Macros

```cpp
ESP32_HOST_MIDI_HAS_USB    // 1 if chip supports USB OTG (S2, S3, P4)
ESP32_HOST_MIDI_HAS_BLE    // 1 if CONFIG_BT_ENABLED
ESP32_HOST_MIDI_HAS_PSRAM  // 1 if CONFIG_SPIRAM or CONFIG_SPIRAM_SUPPORT
ESP32_HOST_MIDI_HAS_ETH_MAC // 1 if ESP32-P4 (native Ethernet MAC)
```

---

## Usage Notes

- `midiHandler.task()` must be called in **every** `loop()`, without long blocking calls
- `addTransport()` must be called **before** `begin()`
- The maximum number of external transports is **4** (built-ins do not count)
- Ring buffers are thread-safe with `portMUX` -- safe for FreeRTOS
- The queue (`getQueue()`) is only valid within the current iteration -- do not hold references beyond the current loop
