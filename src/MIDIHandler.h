#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "MIDIHandlerConfig.h"
#include "MIDITransport.h"
#include "MIDI2Support.h"

// --- Feature detection macros ---
// If ESP32_Host_MIDI.h was included first, these are already defined.
// Otherwise, detect features from ESP-IDF / SDK configuration.
#ifndef ESP32_HOST_MIDI_HAS_USB
  #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
      defined(CONFIG_IDF_TARGET_ESP32P4)
    #define ESP32_HOST_MIDI_HAS_USB 1
  #else
    #define ESP32_HOST_MIDI_HAS_USB 0
  #endif
#endif

#ifndef ESP32_HOST_MIDI_HAS_BLE
  #if defined(CONFIG_BT_ENABLED)
    #define ESP32_HOST_MIDI_HAS_BLE 1
  #else
    #define ESP32_HOST_MIDI_HAS_BLE 0
  #endif
#endif

#ifndef ESP32_HOST_MIDI_HAS_PSRAM
  #if defined(CONFIG_SPIRAM) || defined(CONFIG_SPIRAM_SUPPORT)
    #define ESP32_HOST_MIDI_HAS_PSRAM 1
  #else
    #define ESP32_HOST_MIDI_HAS_PSRAM 0
  #endif
#endif

#ifndef ESP32_HOST_MIDI_HAS_ETH_MAC
  #if defined(CONFIG_IDF_TARGET_ESP32P4)
    #define ESP32_HOST_MIDI_HAS_ETH_MAC 1
  #else
    #define ESP32_HOST_MIDI_HAS_ETH_MAC 0
  #endif
#endif

#if ESP32_HOST_MIDI_HAS_USB && !defined(ESP32_HOST_MIDI_NO_USB_HOST)
  #include "USBConnection.h"
#endif

#if ESP32_HOST_MIDI_HAS_BLE
  #include "BLEConnection.h"
#endif

// MIDI status byte values — matches the upper nibble of MIDI 1.0 status bytes.
// Use these with MIDIEventData::statusCode for type-safe, zero-cost comparisons.
enum MIDIStatus : uint8_t {
    MIDI_NOTE_OFF          = 0x80,
    MIDI_NOTE_ON           = 0x90,
    MIDI_POLY_PRESSURE     = 0xA0,
    MIDI_CONTROL_CHANGE    = 0xB0,
    MIDI_PROGRAM_CHANGE    = 0xC0,
    MIDI_CHANNEL_PRESSURE  = 0xD0,
    MIDI_PITCH_BEND        = 0xE0,
};

// Structure representing a parsed MIDI event using MIDI 1.0 terminology.
struct MIDIEventData {
  int index;                // Global event counter
  int msgIndex;             // Index linking NoteOn/NoteOff pairs
  unsigned long timestamp;  // Timestamp in milliseconds (millis())
  unsigned long delay;      // Delta time (ms) since previous event
  int chordIndex;           // Chord grouping index (simultaneous notes share the same index)

  // --- MIDI spec compliant fields (v5.2+) ---
  MIDIStatus statusCode;    // Status as enum (MIDI_NOTE_ON, MIDI_CONTROL_CHANGE, etc.)
  uint8_t channel0;         // MIDI channel 0-15 (MIDI spec convention)
  uint8_t noteNumber;       // MIDI note number 0-127 (or controller number for CC)
  uint16_t velocity16;      // 16-bit velocity (MIDI 2.0 resolution); MIDI 1.0 input scaled via MIDI2Scaler
  uint8_t velocity7;        // 7-bit velocity (original MIDI 1.0 value)
  uint32_t pitchBend32;     // 32-bit pitch bend (MIDI 2.0, center = 0x80000000)
  uint16_t pitchBend14;     // 14-bit pitch bend (0-16383, center = 8192)

  // --- Deprecated fields (kept for backward compatibility, will be removed in v6.0) ---
  int channel;              // MIDI channel (1-16) — deprecated: use channel0 (0-15)
  std::string status;       // "NoteOn", "NoteOff", etc. — deprecated: use statusCode
  int note;                 // MIDI note number — deprecated: use noteNumber
  std::string noteName;     // "C", "D#" — deprecated: use MIDIHandler::noteName()
  std::string noteOctave;   // "C4", "D#5" — deprecated: use MIDIHandler::noteWithOctave()
  int velocity;             // 7-bit velocity — deprecated: use velocity16 or velocity7
  int pitchBend;            // 14-bit pitch bend — deprecated: use pitchBend32 or pitchBend14
};

// Structure representing a complete SysEx message (0xF0 ... payload ... 0xF7).
// Stored in a separate queue from MIDIEventData to avoid breaking existing API.
struct MIDISysExEvent {
  int index;                      // Global SysEx counter
  unsigned long timestamp;        // Timestamp in milliseconds (millis())
  std::vector<uint8_t> data;      // Complete message including 0xF0 and 0xF7
};

class MIDIHandler {
public:
  MIDIHandler();
  ~MIDIHandler();

  void begin();
  void begin(const MIDIHandlerConfig& config);
  void task();
  void enableHistory(int capacity);
  void addEvent(const MIDIEventData& event);
  void processQueue();
  void setQueueLimit(int maxEvents);
  const std::deque<MIDIEventData>& getQueue() const;

  void handleMidiMessage(const uint8_t* data, size_t length);

  // Debug callback — called with raw MIDI bytes before parsing.
  // Set to nullptr to disable. Signature: (rawData, rawLength, midiBytes3)
  typedef void (*RawMidiCallback)(const uint8_t* raw, size_t rawLen,
                                   const uint8_t* midi3);
  void setRawMidiCallback(RawMidiCallback cb) { rawMidiCb = cb; }

  // Static helpers (zero-allocation, v5.2+)
  static const char* noteName(uint8_t noteNumber);
  static int noteOctave(uint8_t noteNumber);
  static const char* noteWithOctave(uint8_t noteNumber, char* buf, size_t bufLen);
  static const char* statusName(MIDIStatus code);

  std::string getActiveNotesString() const;
  std::string getActiveNotes() const;
  std::vector<std::string> getActiveNotesVector() const;
  size_t getActiveNotesCount() const;
  void fillActiveNotes(bool out[128]) const;
  void clearActiveNotesNow();
  void clearQueue();

  // Register an external transport (ESP-NOW, RTP-MIDI, custom, etc.).
  // The transport must already be initialized (begin() called) before adding.
  // MIDIHandler will call task() on it and receive data via callbacks.
  void addTransport(MIDITransport* transport);

  // MIDI Output — send via any transport that supports sending.
  // channel: 1-16. Returns true if any transport sent the message.
  bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
  bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
  bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value);
  bool sendProgramChange(uint8_t channel, uint8_t program);
  bool sendPitchBend(uint8_t channel, int value);  // value: -8192 to 8191
  bool sendRaw(const uint8_t* data, size_t length);
  bool sendBleRaw(const uint8_t* data, size_t length);  // backward compat alias

  // SysEx API — opt-in, does not affect existing event queue.
  const std::deque<MIDISysExEvent>& getSysExQueue() const;
  void clearSysExQueue();
  bool sendSysEx(const uint8_t* data, size_t length);

  typedef void (*SysExCallback)(const uint8_t* data, size_t length);
  void setSysExCallback(SysExCallback cb) { sysExCb = cb; }

#if ESP32_HOST_MIDI_HAS_BLE
  bool isBleConnected() const;
#endif

  // Chord event utility methods:
  int lastChord(const std::deque<MIDIEventData>& queue) const;
  std::vector<std::string> getChord(int chord, const std::deque<MIDIEventData>& queue, const std::vector<std::string>& fields = { "all" }, bool includeLabels = false) const;
  std::vector<std::string> getAnswer(const std::string& field = "all", bool includeLabels = false) const;
  std::vector<std::string> getAnswer(const std::vector<std::string>& fields, bool includeLabels = false) const;

private:
  MIDIHandlerConfig config;
  RawMidiCallback rawMidiCb = nullptr;

  std::deque<MIDIEventData> eventQueue;
  int maxEvents;
  int globalIndex;
  int nextMsgIndex;
  unsigned long lastTimestamp;
  unsigned long lastNoteOnTimestamp;

  std::unordered_map<int, int> activeNotes;
  std::unordered_map<int, int> activeMsgIndex;

  int nextChordIndex;
  int currentChordIndex;

  // History buffer (PSRAM when available, heap otherwise)
  MIDIEventData* historyQueue;
  int historyQueueCapacity;
  int historyQueueSize;
  int historyQueueHead;

  void expandHistoryQueue();

  std::string getNoteName(int note) const;
  std::string getNoteWithOctave(int note) const;

  // --- Transport abstraction ---
  static const int MAX_TRANSPORTS = 4;
  MIDITransport* transports[MAX_TRANSPORTS];
  int transportCount;

  void registerTransport(MIDITransport* t);
  static void _onTransportMidiData(void* ctx, const uint8_t* data, size_t len);
  static void _onTransportDisconnected(void* ctx);
  static void _onTransportSysExData(void* ctx, const uint8_t* data, size_t len);

  // SysEx
  std::deque<MIDISysExEvent> sysexQueue;
  int sysexGlobalIndex = 0;
  SysExCallback sysExCb = nullptr;
  void handleSysExMessage(const uint8_t* data, size_t length);

  // Built-in transports (owned by MIDIHandler, registered automatically in begin())
#if ESP32_HOST_MIDI_HAS_USB && !defined(ESP32_HOST_MIDI_NO_USB_HOST)
  USBConnection usbTransport;
#endif
#if ESP32_HOST_MIDI_HAS_BLE
  BLEConnection bleTransport;
#endif
};

extern MIDIHandler midiHandler;

#endif  // MIDI_HANDLER_H
