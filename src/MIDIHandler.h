#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "MIDIHandlerConfig.h"

// --- Feature detection macros ---
// If ESP32_Host_MIDI.h was included first, these are already defined.
// Otherwise, detect features from ESP-IDF / SDK configuration.
#ifndef ESP32_HOST_MIDI_HAS_USB
  #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
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

#if ESP32_HOST_MIDI_HAS_USB
  #include "USBConnection.h"
#endif

#if ESP32_HOST_MIDI_HAS_BLE
  #include "BLEConnection.h"
#endif

// Structure representing a parsed MIDI event using MIDI 1.0 terminology.
struct MIDIEventData {
  int index;                // Global event counter
  int msgIndex;             // Index linking NoteOn/NoteOff pairs
  unsigned long timestamp;  // Timestamp in milliseconds (millis())
  unsigned long delay;      // Delta time (ms) since previous event
  int channel;              // MIDI channel (1-16)
  std::string status;       // Status type: "NoteOn", "NoteOff", "ControlChange", "ProgramChange", "PitchBend", "ChannelPressure"
  int note;                 // MIDI note number (or controller number for ControlChange)
  std::string noteName;     // Musical note name (e.g., "C", "D#") — empty for non-note messages
  std::string noteOctave;   // Note with octave (e.g., "C4", "D#5") — empty for non-note messages
  int velocity;             // Velocity (or CC value, program number, or pressure value)
  int chordIndex;           // Chord grouping index (simultaneous notes share the same index)
  int pitchBend;            // Pitch Bend value (14-bit, 0-16383, center = 8192). 0 for other types.
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

  std::string getActiveNotesString() const;
  std::string getActiveNotes() const;
  std::vector<std::string> getActiveNotesVector() const;
  size_t getActiveNotesCount() const;
  void fillActiveNotes(bool out[128]) const;
  void clearActiveNotesNow();
  void clearQueue();

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

#if ESP32_HOST_MIDI_HAS_USB
  class MyUSBConnection : public USBConnection {
  public:
    MyUSBConnection(MIDIHandler* handler)
      : handler(handler) {}
    virtual void onMidiDataReceived(const uint8_t* data, size_t length) override {
      handler->handleMidiMessage(data, length);
    }
  private:
    MIDIHandler* handler;
  };
  MyUSBConnection usbCon;
#endif

#if ESP32_HOST_MIDI_HAS_BLE
  class MyBLEConnection : public BLEConnection {
  public:
    MyBLEConnection(MIDIHandler* handler)
      : handler(handler) {}
    virtual void onMidiDataReceived(const uint8_t* data, size_t length) override {
      handler->handleMidiMessage(data, length);
    }
  private:
    MIDIHandler* handler;
  };
  MyBLEConnection bleCon;
#endif
};

extern MIDIHandler midiHandler;

#endif  // MIDI_HANDLER_H
