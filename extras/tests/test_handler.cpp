// ESP32_Host_MIDI — MIDIHandler test suite
// Tests the core event processing pipeline including v5.2 spec-compliant fields.
// Build:
//   g++ -std=c++17 -Iextras/tests/stub -Isrc -Wall -Wextra -Wno-unused-parameter \
//       -Wno-comment -DESP32_HOST_MIDI_NO_USB_HOST \
//       -o extras/tests/test_handler extras/tests/test_handler.cpp src/MIDIHandler.cpp

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>

// Stubs
#include "stub/Arduino.h"

unsigned long g_fakeMillis = 0;
FakeSerial Serial;

// Include after stubs
#include "../../src/MIDIHandler.h"

// ---------------------------------------------------------------------------
// Minimal test framework (same as test_native.cpp)
// ---------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-56s", name); } while(0)

#define PASS() \
    do { printf("OK\n"); ++g_pass; } while(0)

#define FAIL(msg) \
    do { printf("FAIL — %s\n", msg); ++g_fail; return; } while(0)

#define ASSERT(expr) \
    do { if (!(expr)) { printf("FAIL — " #expr "\n"); ++g_fail; return; } } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { printf("FAIL — %s != %s (got %ld vs %ld)\n", #a, #b, (long)(a), (long)(b)); ++g_fail; return; } } while(0)

// ---------------------------------------------------------------------------
// Helper: feed raw MIDI into handler and return the last event
// ---------------------------------------------------------------------------

static MIDIEventData feedMidi(MIDIHandler& h, uint8_t status, uint8_t d1, uint8_t d2 = 0) {
    // Always use USB-MIDI format: 4 bytes (CIN + 3 MIDI bytes).
    // handleMidiMessage skips byte[0] when length >= 4.
    uint8_t cin = (status >> 4) & 0x0F;
    uint8_t buf[4] = { cin, status, d1, d2 };
    h.handleMidiMessage(buf, 4);
    auto& q = h.getQueue();
    return q.back();
}

// ---------------------------------------------------------------------------
// Test: NoteOn — new and deprecated fields
// ---------------------------------------------------------------------------

void test_noteon_fields() {
    printf("\n[NoteOn Fields]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 1000;

    TEST("statusCode == MIDI_NOTE_ON");
    auto ev = feedMidi(h, 0x90, 60, 100);  // NoteOn ch0 note=60 vel=100
    ASSERT(ev.statusCode == MIDI_NOTE_ON);
    PASS();

    TEST("channel0 == 0 (ch1 in MIDI)");
    ASSERT_EQ(ev.channel0, 0);
    PASS();

    TEST("noteNumber == 60");
    ASSERT_EQ(ev.noteNumber, 60);
    PASS();

    TEST("velocity7 == 100");
    ASSERT_EQ(ev.velocity7, 100);
    PASS();

    TEST("velocity16 == scale7to16(100)");
    ASSERT_EQ(ev.velocity16, MIDI2Scaler::scale7to16(100));
    PASS();

    TEST("pitchBend14 == 0, pitchBend32 == center");
    ASSERT_EQ(ev.pitchBend14, 0);
    ASSERT(ev.pitchBend32 == 0x80000000U);
    PASS();

    // Deprecated fields
    TEST("deprecated: channel == 1");
    ASSERT_EQ(ev.channel, 1);
    PASS();

    TEST("deprecated: status == \"NoteOn\"");
    ASSERT(ev.status == "NoteOn");
    PASS();

    TEST("deprecated: note == 60");
    ASSERT_EQ(ev.note, 60);
    PASS();

    TEST("deprecated: velocity == 100");
    ASSERT_EQ(ev.velocity, 100);
    PASS();

    TEST("deprecated: noteName == \"C\"");
    ASSERT(ev.noteName == "C");
    PASS();

    TEST("deprecated: noteOctave == \"C4\"");
    ASSERT(ev.noteOctave == "C4");
    PASS();
}

// ---------------------------------------------------------------------------
// Test: NoteOff — explicit 0x80 message
// ---------------------------------------------------------------------------

void test_noteoff_fields() {
    printf("\n[NoteOff Fields]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 1000;

    // First send NoteOn to register the note
    feedMidi(h, 0x90, 64, 80);
    g_fakeMillis = 1050;

    TEST("explicit NoteOff (0x80) statusCode");
    auto ev = feedMidi(h, 0x80, 64, 0);
    ASSERT(ev.statusCode == MIDI_NOTE_OFF);
    PASS();

    TEST("channel0 correct for ch3 (status 0x82)");
    MIDIHandler h2;
    h2.begin();
    feedMidi(h2, 0x92, 60, 100); // NoteOn ch2
    auto ev2 = feedMidi(h2, 0x82, 60, 0); // NoteOff ch2
    ASSERT_EQ(ev2.channel0, 2);
    ASSERT_EQ(ev2.channel, 3); // deprecated: 1-based
    PASS();
}

// ---------------------------------------------------------------------------
// Test: NoteOn with velocity 0 → treated as NoteOff
// ---------------------------------------------------------------------------

void test_noteon_vel0_is_noteoff() {
    printf("\n[NoteOn vel=0 → NoteOff]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 1000;

    feedMidi(h, 0x90, 60, 100);  // NoteOn
    g_fakeMillis = 1100;

    TEST("NoteOn vel=0 → statusCode == MIDI_NOTE_OFF");
    auto ev = feedMidi(h, 0x90, 60, 0);
    ASSERT(ev.statusCode == MIDI_NOTE_OFF);
    PASS();

    TEST("deprecated status == \"NoteOff\"");
    ASSERT(ev.status == "NoteOff");
    PASS();

    TEST("velocity7 == 0, velocity16 == 0");
    ASSERT_EQ(ev.velocity7, 0);
    ASSERT_EQ(ev.velocity16, 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: ControlChange
// ---------------------------------------------------------------------------

void test_cc_fields() {
    printf("\n[ControlChange Fields]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 2000;

    TEST("statusCode == MIDI_CONTROL_CHANGE");
    auto ev = feedMidi(h, 0xB5, 7, 127);  // CC7 (Volume) ch5 val=127
    ASSERT(ev.statusCode == MIDI_CONTROL_CHANGE);
    PASS();

    TEST("channel0 == 5");
    ASSERT_EQ(ev.channel0, 5);
    PASS();

    TEST("noteNumber == 7 (controller number)");
    ASSERT_EQ(ev.noteNumber, 7);
    PASS();

    TEST("velocity7 == 127 (CC value)");
    ASSERT_EQ(ev.velocity7, 127);
    PASS();

    TEST("velocity16 == scale7to16(127) == 0xFFFF");
    ASSERT_EQ(ev.velocity16, 0xFFFF);
    PASS();

    TEST("deprecated: status == \"ControlChange\"");
    ASSERT(ev.status == "ControlChange");
    PASS();

    TEST("deprecated: channel == 6 (1-based)");
    ASSERT_EQ(ev.channel, 6);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: ProgramChange
// ---------------------------------------------------------------------------

void test_pc_fields() {
    printf("\n[ProgramChange Fields]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 3000;

    auto ev = feedMidi(h, 0xC0, 42);

    TEST("statusCode == MIDI_PROGRAM_CHANGE");
    ASSERT(ev.statusCode == MIDI_PROGRAM_CHANGE);
    PASS();

    TEST("noteNumber == 42 (program number)");
    ASSERT_EQ(ev.noteNumber, 42);
    PASS();

    TEST("velocity7 == 0, velocity16 == 0");
    ASSERT_EQ(ev.velocity7, 0);
    ASSERT_EQ(ev.velocity16, 0);
    PASS();

    TEST("deprecated: status == \"ProgramChange\"");
    ASSERT(ev.status == "ProgramChange");
    PASS();
}

// ---------------------------------------------------------------------------
// Test: ChannelPressure (Aftertouch)
// ---------------------------------------------------------------------------

void test_cp_fields() {
    printf("\n[ChannelPressure Fields]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 4000;

    auto ev = feedMidi(h, 0xD0, 80);

    TEST("statusCode == MIDI_CHANNEL_PRESSURE");
    ASSERT(ev.statusCode == MIDI_CHANNEL_PRESSURE);
    PASS();

    TEST("noteNumber == 0 (no note for CP)");
    ASSERT_EQ(ev.noteNumber, 0);
    PASS();

    TEST("velocity7 == 80 (pressure value)");
    ASSERT_EQ(ev.velocity7, 80);
    PASS();

    TEST("velocity16 == scale7to16(80)");
    ASSERT_EQ(ev.velocity16, MIDI2Scaler::scale7to16(80));
    PASS();

    TEST("deprecated: status == \"ChannelPressure\"");
    ASSERT(ev.status == "ChannelPressure");
    PASS();
}

// ---------------------------------------------------------------------------
// Test: PitchBend
// ---------------------------------------------------------------------------

void test_pb_fields() {
    printf("\n[PitchBend Fields]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 5000;

    // Pitch bend center = 8192 = LSB 0x00, MSB 0x40
    TEST("center pitch bend (8192)");
    auto ev = feedMidi(h, 0xE0, 0x00, 0x40);
    ASSERT(ev.statusCode == MIDI_PITCH_BEND);
    ASSERT_EQ(ev.pitchBend14, 8192);
    ASSERT_EQ(ev.pitchBend, 8192); // deprecated
    PASS();

    TEST("pitchBend32 scaled from center");
    uint32_t expected32 = MIDI2Scaler::scale14to32(8192);
    ASSERT(ev.pitchBend32 == expected32);
    PASS();

    TEST("max pitch bend (16383)");
    auto ev2 = feedMidi(h, 0xE0, 0x7F, 0x7F);
    ASSERT_EQ(ev2.pitchBend14, 16383);
    PASS();

    TEST("min pitch bend (0)");
    auto ev3 = feedMidi(h, 0xE0, 0x00, 0x00);
    ASSERT_EQ(ev3.pitchBend14, 0);
    PASS();

    TEST("velocity7/16 == 0 for PitchBend");
    ASSERT_EQ(ev.velocity7, 0);
    ASSERT_EQ(ev.velocity16, 0);
    PASS();

    TEST("channel0 for PitchBend on ch9");
    auto ev4 = feedMidi(h, 0xE9, 0x00, 0x40);
    ASSERT_EQ(ev4.channel0, 9);
    ASSERT_EQ(ev4.channel, 10); // deprecated: 1-based
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Static helpers — noteName, noteOctave, noteWithOctave, statusName
// ---------------------------------------------------------------------------

void test_static_helpers() {
    printf("\n[Static Helpers]\n");

    TEST("noteName(60) == \"C\"");
    ASSERT(strcmp(MIDIHandler::noteName(60), "C") == 0);
    PASS();

    TEST("noteName(61) == \"C#\"");
    ASSERT(strcmp(MIDIHandler::noteName(61), "C#") == 0);
    PASS();

    TEST("noteName(69) == \"A\" (A4 = 440Hz)");
    ASSERT(strcmp(MIDIHandler::noteName(69), "A") == 0);
    PASS();

    TEST("noteOctave(60) == 4");
    ASSERT_EQ(MIDIHandler::noteOctave(60), 4);
    PASS();

    TEST("noteOctave(0) == -1");
    ASSERT_EQ(MIDIHandler::noteOctave(0), -1);
    PASS();

    TEST("noteOctave(127) == 9");
    ASSERT_EQ(MIDIHandler::noteOctave(127), 9);
    PASS();

    TEST("noteWithOctave(60) == \"C4\"");
    char buf[8];
    MIDIHandler::noteWithOctave(60, buf, sizeof(buf));
    ASSERT(strcmp(buf, "C4") == 0);
    PASS();

    TEST("noteWithOctave(21) == \"A0\" (lowest piano key)");
    MIDIHandler::noteWithOctave(21, buf, sizeof(buf));
    ASSERT(strcmp(buf, "A0") == 0);
    PASS();

    TEST("noteWithOctave(108) == \"C8\" (highest piano key)");
    MIDIHandler::noteWithOctave(108, buf, sizeof(buf));
    ASSERT(strcmp(buf, "C8") == 0);
    PASS();

    TEST("statusName(MIDI_NOTE_ON) == \"NoteOn\"");
    ASSERT(strcmp(MIDIHandler::statusName(MIDI_NOTE_ON), "NoteOn") == 0);
    PASS();

    TEST("statusName(MIDI_NOTE_OFF) == \"NoteOff\"");
    ASSERT(strcmp(MIDIHandler::statusName(MIDI_NOTE_OFF), "NoteOff") == 0);
    PASS();

    TEST("statusName(MIDI_CONTROL_CHANGE) == \"ControlChange\"");
    ASSERT(strcmp(MIDIHandler::statusName(MIDI_CONTROL_CHANGE), "ControlChange") == 0);
    PASS();

    TEST("statusName(MIDI_PITCH_BEND) == \"PitchBend\"");
    ASSERT(strcmp(MIDIHandler::statusName(MIDI_PITCH_BEND), "PitchBend") == 0);
    PASS();

    TEST("statusName(MIDI_PROGRAM_CHANGE) == \"ProgramChange\"");
    ASSERT(strcmp(MIDIHandler::statusName(MIDI_PROGRAM_CHANGE), "ProgramChange") == 0);
    PASS();

    TEST("statusName(MIDI_CHANNEL_PRESSURE) == \"ChannelPressure\"");
    ASSERT(strcmp(MIDIHandler::statusName(MIDI_CHANNEL_PRESSURE), "ChannelPressure") == 0);
    PASS();

    TEST("statusName invalid → \"Unknown\"");
    ASSERT(strcmp(MIDIHandler::statusName(static_cast<MIDIStatus>(0xFF)), "Unknown") == 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Multi-channel — all 16 MIDI channels
// ---------------------------------------------------------------------------

void test_all_channels() {
    printf("\n[All 16 Channels]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 6000;

    for (int ch = 0; ch < 16; ch++) {
        char name[64];
        snprintf(name, sizeof(name), "channel0 == %d for status 0x%02X", ch, 0x90 | ch);
        TEST(name);
        auto ev = feedMidi(h, 0x90 | ch, 60, 100);
        ASSERT_EQ(ev.channel0, ch);
        ASSERT_EQ(ev.channel, ch + 1); // deprecated
        PASS();
        // Release note
        feedMidi(h, 0x80 | ch, 60, 0);
    }
}

// ---------------------------------------------------------------------------
// Test: Velocity scaling boundary values
// ---------------------------------------------------------------------------

void test_velocity_scaling() {
    printf("\n[Velocity Scaling]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 7000;

    TEST("vel=1 → velocity16 == scale7to16(1)");
    auto ev = feedMidi(h, 0x90, 60, 1);
    ASSERT_EQ(ev.velocity7, 1);
    ASSERT_EQ(ev.velocity16, MIDI2Scaler::scale7to16(1));
    PASS();
    feedMidi(h, 0x80, 60, 0);

    TEST("vel=64 (mezzo-forte) → scaled correctly");
    auto ev2 = feedMidi(h, 0x90, 60, 64);
    ASSERT_EQ(ev2.velocity7, 64);
    ASSERT_EQ(ev2.velocity16, MIDI2Scaler::scale7to16(64));
    PASS();
    feedMidi(h, 0x80, 60, 0);

    TEST("vel=127 (fff) → velocity16 == 0xFFFF");
    auto ev3 = feedMidi(h, 0x90, 60, 127);
    ASSERT_EQ(ev3.velocity7, 127);
    ASSERT_EQ(ev3.velocity16, 0xFFFF);
    PASS();
    feedMidi(h, 0x80, 60, 0);
}

// ---------------------------------------------------------------------------
// Test: Queue management
// ---------------------------------------------------------------------------

void test_queue() {
    printf("\n[Queue Management]\n");

    MIDIHandler h;
    MIDIHandlerConfig cfg;
    cfg.maxEvents = 5;
    h.begin(cfg);
    g_fakeMillis = 8000;

    TEST("queue respects maxEvents limit");
    for (int i = 0; i < 10; i++) {
        feedMidi(h, 0x90, 60 + (i % 12), 100);
        feedMidi(h, 0x80, 60 + (i % 12), 0);
    }
    ASSERT(h.getQueue().size() <= 5);
    PASS();

    TEST("clearQueue empties the queue");
    h.clearQueue();
    ASSERT(h.getQueue().empty());
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Active notes tracking
// ---------------------------------------------------------------------------

void test_active_notes() {
    printf("\n[Active Notes]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 9000;

    TEST("no active notes initially");
    ASSERT_EQ(h.getActiveNotesCount(), 0);
    PASS();

    TEST("NoteOn adds to active notes");
    feedMidi(h, 0x90, 60, 100);
    feedMidi(h, 0x90, 64, 100);
    feedMidi(h, 0x90, 67, 100);
    ASSERT_EQ(h.getActiveNotesCount(), 3);
    PASS();

    TEST("NoteOff removes from active notes");
    feedMidi(h, 0x80, 64, 0);
    ASSERT_EQ(h.getActiveNotesCount(), 2);
    PASS();

    TEST("NoteOn vel=0 also removes");
    feedMidi(h, 0x90, 60, 0);
    ASSERT_EQ(h.getActiveNotesCount(), 1);
    PASS();

    TEST("clearActiveNotesNow empties all");
    h.clearActiveNotesNow();
    ASSERT_EQ(h.getActiveNotesCount(), 0);
    PASS();

    TEST("fillActiveNotes fills boolean array");
    feedMidi(h, 0x90, 48, 100);
    feedMidi(h, 0x90, 72, 100);
    bool notes[128] = {};
    h.fillActiveNotes(notes);
    ASSERT(notes[48] == true);
    ASSERT(notes[72] == true);
    ASSERT(notes[60] == false);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Chord detection
// ---------------------------------------------------------------------------

void test_chord_detection() {
    printf("\n[Chord Detection]\n");

    MIDIHandlerConfig cfg;
    cfg.chordTimeWindow = 50;  // 50ms window
    MIDIHandler h;
    h.begin(cfg);

    TEST("simultaneous notes share chordIndex");
    g_fakeMillis = 10000;
    auto ev1 = feedMidi(h, 0x90, 60, 100);
    g_fakeMillis = 10010; // 10ms later — within window
    auto ev2 = feedMidi(h, 0x90, 64, 100);
    g_fakeMillis = 10020; // 20ms later — still within window
    auto ev3 = feedMidi(h, 0x90, 67, 100);
    ASSERT_EQ(ev1.chordIndex, ev2.chordIndex);
    ASSERT_EQ(ev2.chordIndex, ev3.chordIndex);
    PASS();

    TEST("notes after window get new chordIndex");
    // Release all
    feedMidi(h, 0x80, 60, 0);
    feedMidi(h, 0x80, 64, 0);
    feedMidi(h, 0x80, 67, 0);
    g_fakeMillis = 10200; // 200ms later — new chord
    auto ev4 = feedMidi(h, 0x90, 72, 100);
    ASSERT(ev4.chordIndex != ev1.chordIndex);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Velocity threshold filter
// ---------------------------------------------------------------------------

void test_velocity_threshold() {
    printf("\n[Velocity Threshold]\n");

    MIDIHandlerConfig cfg;
    cfg.velocityThreshold = 20;
    MIDIHandler h;
    h.begin(cfg);
    g_fakeMillis = 11000;

    TEST("ghost note below threshold is ignored");
    size_t before = h.getQueue().size();
    // Don't use feedMidi() here — it calls q.back() which is UB on empty queue
    uint8_t ghost[4] = { 0x09, 0x90, 60, 10 }; // vel=10 < threshold=20
    h.handleMidiMessage(ghost, 4);
    ASSERT_EQ(h.getQueue().size(), before); // queue unchanged
    PASS();

    TEST("note at threshold is accepted");
    auto ev1 = feedMidi(h, 0x90, 60, 20);
    ASSERT_EQ(h.getQueue().size(), before + 1);
    PASS();

    TEST("note above threshold is accepted");
    auto ev2 = feedMidi(h, 0x90, 64, 100);
    ASSERT_EQ(h.getQueue().size(), before + 2);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Event index and timestamp
// ---------------------------------------------------------------------------

void test_event_metadata() {
    printf("\n[Event Metadata]\n");

    MIDIHandler h;
    h.begin();

    TEST("index increments for each event");
    g_fakeMillis = 12000;
    auto ev1 = feedMidi(h, 0x90, 60, 100);
    g_fakeMillis = 12050;
    auto ev2 = feedMidi(h, 0x80, 60, 0);
    ASSERT(ev2.index == ev1.index + 1);
    PASS();

    TEST("timestamp reflects millis()");
    ASSERT_EQ(ev1.timestamp, 12000UL);
    ASSERT_EQ(ev2.timestamp, 12050UL);
    PASS();

    TEST("delay is delta from previous event");
    ASSERT_EQ(ev2.delay, 50UL);
    PASS();

    TEST("msgIndex links NoteOn/NoteOff pairs");
    ASSERT(ev1.msgIndex == ev2.msgIndex);
    ASSERT(ev1.msgIndex > 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: BLE/raw MIDI format (3 bytes without CIN prefix)
// ---------------------------------------------------------------------------

void test_raw_midi_format() {
    printf("\n[Raw MIDI Format (BLE/3-byte)]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 13000;

    TEST("3-byte raw MIDI (no CIN) → parsed correctly");
    uint8_t raw[3] = { 0x90, 60, 100 };
    h.handleMidiMessage(raw, 3);
    auto ev = h.getQueue().back();
    ASSERT(ev.statusCode == MIDI_NOTE_ON);
    ASSERT_EQ(ev.noteNumber, 60);
    ASSERT_EQ(ev.velocity7, 100);
    PASS();

    TEST("2-byte raw MIDI (Program Change) → parsed correctly");
    uint8_t raw2[2] = { 0xC0, 42 };
    h.handleMidiMessage(raw2, 2);
    auto ev2 = h.getQueue().back();
    ASSERT(ev2.statusCode == MIDI_PROGRAM_CHANGE);
    ASSERT_EQ(ev2.noteNumber, 42);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Edge cases
// ---------------------------------------------------------------------------

void test_edge_cases() {
    printf("\n[Edge Cases]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 14000;

    TEST("1-byte message is ignored (no crash)");
    uint8_t one[1] = { 0x90 };
    h.handleMidiMessage(one, 1);
    PASS();

    TEST("0-byte message is ignored (no crash)");
    h.handleMidiMessage(nullptr, 0);
    PASS();

    TEST("note 0 (C-1) has correct name");
    auto ev = feedMidi(h, 0x90, 0, 100);
    ASSERT_EQ(ev.noteNumber, 0);
    char buf[8];
    MIDIHandler::noteWithOctave(0, buf, sizeof(buf));
    ASSERT(strcmp(buf, "C-1") == 0);
    PASS();
    feedMidi(h, 0x80, 0, 0);

    TEST("note 127 (G9) has correct name");
    auto ev2 = feedMidi(h, 0x90, 127, 100);
    ASSERT_EQ(ev2.noteNumber, 127);
    MIDIHandler::noteWithOctave(127, buf, sizeof(buf));
    ASSERT(strcmp(buf, "G9") == 0);
    PASS();
    feedMidi(h, 0x80, 127, 0);

    TEST("NoteOff for non-active note doesn't crash");
    feedMidi(h, 0x80, 99, 0);
    PASS();

    TEST("duplicate NoteOn for same note");
    feedMidi(h, 0x90, 60, 100);
    feedMidi(h, 0x90, 60, 80); // second NoteOn, same note
    ASSERT_EQ(h.getActiveNotesCount(), 1); // still 1 active
    PASS();
    feedMidi(h, 0x80, 60, 0);
}

// ---------------------------------------------------------------------------
// MockTransport — for routing tests
// ---------------------------------------------------------------------------

class MockTransport : public MIDITransport {
public:
    const char* _name;
    bool _connected;
    std::vector<std::vector<uint8_t>> sentMessages;

    MockTransport(const char* name = "Mock", bool connected = true)
        : _name(name), _connected(connected) {}

    void task() override {}
    bool isConnected() const override { return _connected; }
    const char* name() const override { return _name; }

    bool sendMidiMessage(const uint8_t* data, size_t length) override {
        if (!_connected) return false;
        sentMessages.push_back(std::vector<uint8_t>(data, data + length));
        return true;
    }

    void clearSent() { sentMessages.clear(); }

    // Simulate incoming MIDI (calls dispatchMidiData which fires the callback)
    void injectMidi(uint8_t status, uint8_t d1, uint8_t d2 = 0) {
        uint8_t cin = (status >> 4) & 0x0F;
        uint8_t buf[4] = { cin, status, d1, d2 };
        dispatchMidiData(buf, 4);
    }
};

// Helper: feed MIDI via a specific transport and return last event
static MIDIEventData feedMidiVia(MIDIHandler& h, MockTransport& t,
                                  uint8_t status, uint8_t d1, uint8_t d2 = 0) {
    t.injectMidi(status, d1, d2);
    h.task();  // drain transport queue
    auto& q = h.getQueue();
    return q.back();
}

// ---------------------------------------------------------------------------
// Test: Source tracking — events carry the transport that received them
// ---------------------------------------------------------------------------

void test_source_tracking() {
    printf("\n[Source Tracking]\n");

    MIDIHandler h;
    MIDIHandlerConfig cfg;
    cfg.maxEvents = 20;
    h.begin(cfg);

    MockTransport usb("USB Host");
    MockTransport ble("BLE Server");
    h.addTransport(&usb);
    h.addTransport(&ble);

    g_fakeMillis = 20000;

    TEST("event from USB has source == &usb");
    auto ev1 = feedMidiVia(h, usb, 0x90, 60, 100);
    ASSERT(ev1.source == &usb);
    PASS();

    TEST("event from BLE has source == &ble");
    auto ev2 = feedMidiVia(h, ble, 0x90, 64, 80);
    ASSERT(ev2.source == &ble);
    PASS();

    TEST("source->name() returns correct name");
    ASSERT(strcmp(ev1.source->name(), "USB Host") == 0);
    ASSERT(strcmp(ev2.source->name(), "BLE Server") == 0);
    PASS();

    TEST("direct handleMidiMessage has source == nullptr");
    uint8_t buf[4] = { 0x09, 0x90, 72, 90 };
    h.handleMidiMessage(buf, 4);
    auto& q = h.getQueue();
    auto ev3 = q.back();
    ASSERT(ev3.source == nullptr);
    PASS();

    TEST("CC event has correct source");
    auto ev4 = feedMidiVia(h, usb, 0xB0, 1, 64);
    ASSERT(ev4.source == &usb);
    ASSERT(ev4.statusCode == MIDI_CONTROL_CHANGE);
    PASS();

    TEST("PitchBend event has correct source");
    auto ev5 = feedMidiVia(h, ble, 0xE0, 0, 64);
    ASSERT(ev5.source == &ble);
    ASSERT(ev5.statusCode == MIDI_PITCH_BEND);
    PASS();

    TEST("ProgramChange event has correct source");
    auto ev6 = feedMidiVia(h, usb, 0xC0, 42, 0);
    ASSERT(ev6.source == &usb);
    ASSERT(ev6.statusCode == MIDI_PROGRAM_CHANGE);
    PASS();

    TEST("ChannelPressure event has correct source");
    auto ev7 = feedMidiVia(h, ble, 0xD0, 100, 0);
    ASSERT(ev7.source == &ble);
    ASSERT(ev7.statusCode == MIDI_CHANNEL_PRESSURE);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Targeted send — messages go to specific transport
// ---------------------------------------------------------------------------

void test_targeted_send() {
    printf("\n[Targeted Send]\n");

    MIDIHandler h;
    h.begin();

    MockTransport t1("Synth");
    MockTransport t2("Monitor");
    h.addTransport(&t1);
    h.addTransport(&t2);

    g_fakeMillis = 30000;

    TEST("sendNoteOn to t1 only");
    t1.clearSent(); t2.clearSent();
    h.sendNoteOn(1, 60, 100, &t1);
    ASSERT_EQ(t1.sentMessages.size(), 1);
    ASSERT_EQ(t2.sentMessages.size(), 0);
    PASS();

    TEST("sendNoteOn to t2 only");
    t1.clearSent(); t2.clearSent();
    h.sendNoteOn(1, 60, 100, &t2);
    ASSERT_EQ(t1.sentMessages.size(), 0);
    ASSERT_EQ(t2.sentMessages.size(), 1);
    PASS();

    TEST("sendNoteOn broadcast (nullptr) goes to first");
    t1.clearSent(); t2.clearSent();
    h.sendNoteOn(1, 60, 100);
    // Broadcast sends to first successful transport
    ASSERT(t1.sentMessages.size() == 1 || t2.sentMessages.size() == 1);
    PASS();

    TEST("sendNoteOff targeted");
    t1.clearSent(); t2.clearSent();
    h.sendNoteOff(1, 60, 0, &t2);
    ASSERT_EQ(t1.sentMessages.size(), 0);
    ASSERT_EQ(t2.sentMessages.size(), 1);
    PASS();

    TEST("sendControlChange targeted");
    t1.clearSent(); t2.clearSent();
    h.sendControlChange(1, 7, 127, &t1);
    ASSERT_EQ(t1.sentMessages.size(), 1);
    ASSERT_EQ(t2.sentMessages.size(), 0);
    // Verify bytes: 0xB0, 7, 127
    ASSERT_EQ(t1.sentMessages[0][0], 0xB0);
    ASSERT_EQ(t1.sentMessages[0][1], 7);
    ASSERT_EQ(t1.sentMessages[0][2], 127);
    PASS();

    TEST("sendProgramChange targeted");
    t1.clearSent(); t2.clearSent();
    h.sendProgramChange(2, 42, &t2);
    ASSERT_EQ(t2.sentMessages.size(), 1);
    ASSERT_EQ(t2.sentMessages[0][0], 0xC1); // ch2 = 0xC0 | 1
    ASSERT_EQ(t2.sentMessages[0][1], 42);
    PASS();

    TEST("sendPitchBend targeted");
    t1.clearSent(); t2.clearSent();
    h.sendPitchBend(1, 0, &t1); // center
    ASSERT_EQ(t1.sentMessages.size(), 1);
    // Center = 8192 = 0x2000, LSB=0x00, MSB=0x40
    ASSERT_EQ(t1.sentMessages[0][0], 0xE0);
    ASSERT_EQ(t1.sentMessages[0][1], 0x00);
    ASSERT_EQ(t1.sentMessages[0][2], 0x40);
    PASS();

    TEST("sendRaw targeted");
    t1.clearSent(); t2.clearSent();
    uint8_t raw[3] = { 0x90, 60, 100 };
    h.sendRaw(raw, 3, &t2);
    ASSERT_EQ(t1.sentMessages.size(), 0);
    ASSERT_EQ(t2.sentMessages.size(), 1);
    PASS();

    TEST("targeted send to disconnected transport returns false");
    MockTransport offline("Offline", false);
    h.addTransport(&offline);
    bool result = h.sendNoteOn(1, 60, 100, &offline);
    ASSERT(!result);
    PASS();

    TEST("invalid channel rejected");
    t1.clearSent();
    bool r1 = h.sendNoteOn(0, 60, 100, &t1);
    bool r2 = h.sendNoteOn(17, 60, 100, &t1);
    ASSERT(!r1);
    ASSERT(!r2);
    ASSERT_EQ(t1.sentMessages.size(), 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Transport access API
// ---------------------------------------------------------------------------

void test_transport_access() {
    printf("\n[Transport Access]\n");

    MIDIHandler h;
    h.begin();

    MockTransport t1("Port A");
    MockTransport t2("Port B");
    h.addTransport(&t1);
    h.addTransport(&t2);

    // Built-in transports (USB, BLE) are not available in test build
    // (ESP32_HOST_MIDI_NO_USB_HOST is defined), so count = external only

    TEST("getTransportCount includes added transports");
    ASSERT(h.getTransportCount() >= 2);
    PASS();

    TEST("getTransport returns correct pointer");
    int idx = h.getTransportCount();
    ASSERT(h.getTransport(idx - 2) == &t1);
    ASSERT(h.getTransport(idx - 1) == &t2);
    PASS();

    TEST("getTransport out of bounds returns nullptr");
    ASSERT(h.getTransport(-1) == nullptr);
    ASSERT(h.getTransport(99) == nullptr);
    PASS();

    TEST("transport name() works via getTransport");
    MIDITransport* p = h.getTransport(idx - 1);
    ASSERT(p != nullptr);
    ASSERT(strcmp(p->name(), "Port B") == 0);
    PASS();

    TEST("default MIDITransport::name() returns 'Transport'");
    // MockTransport overrides name(), so test the base directly
    class BareTransport : public MIDITransport {
    public:
        void task() override {}
        bool isConnected() const override { return false; }
    };
    BareTransport bare;
    ASSERT(strcmp(bare.name(), "Transport") == 0);
    PASS();

    TEST("each transport has distinct name via override");
    MockTransport a("UART"), b("ESP-NOW"), c("OSC");
    ASSERT(strcmp(a.name(), "UART") == 0);
    ASSERT(strcmp(b.name(), "ESP-NOW") == 0);
    ASSERT(strcmp(c.name(), "OSC") == 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Routing scenario — al1-24's use case
// ---------------------------------------------------------------------------

void test_routing_scenario() {
    printf("\n[Routing Scenario]\n");

    MIDIHandler h;
    MIDIHandlerConfig cfg;
    cfg.maxEvents = 50;
    h.begin(cfg);

    MockTransport windCtrl("Wind Controller");
    MockTransport footCtrl("Foot Controller");
    MockTransport synth("Synth");
    h.addTransport(&windCtrl);
    h.addTransport(&footCtrl);
    h.addTransport(&synth);

    g_fakeMillis = 40000;

    // Wind controller sends NoteOn + CC02 + PitchBend
    feedMidiVia(h, windCtrl, 0x90, 60, 100);
    feedMidiVia(h, windCtrl, 0xB0, 2, 64);
    feedMidiVia(h, windCtrl, 0xE0, 0, 64);

    // Foot controller sends CC for patch change
    feedMidiVia(h, footCtrl, 0xB0, 0, 5);   // Bank Select
    feedMidiVia(h, footCtrl, 0xC0, 42, 0);  // Program Change

    TEST("all 5 events in queue");
    ASSERT_EQ(h.getQueue().size(), 5);
    PASS();

    TEST("route only to synth, check nothing echoes back");
    synth.clearSent();
    windCtrl.clearSent();
    footCtrl.clearSent();

    auto& q = h.getQueue();
    for (const auto& event : q) {
        if (!event.source || event.source == &synth) continue;

        if (event.statusCode == MIDI_NOTE_ON) {
            h.sendNoteOn(event.channel0 + 1, event.noteNumber, event.velocity7, &synth);
        } else if (event.statusCode == MIDI_CONTROL_CHANGE) {
            // Foot controller CCs go to channel 16
            uint8_t ch = (event.source == &footCtrl) ? 16 : (event.channel0 + 1);
            h.sendControlChange(ch, event.noteNumber, event.velocity7, &synth);
        } else if (event.statusCode == MIDI_PITCH_BEND) {
            h.sendPitchBend(event.channel0 + 1, event.pitchBend14 - 8192, &synth);
        } else if (event.statusCode == MIDI_PROGRAM_CHANGE) {
            h.sendProgramChange(event.channel0 + 1, event.noteNumber, &synth);
        }
    }

    // Synth received all 5 messages
    ASSERT_EQ(synth.sentMessages.size(), 5);
    PASS();

    TEST("wind controller received nothing");
    ASSERT_EQ(windCtrl.sentMessages.size(), 0);
    PASS();

    TEST("foot controller received nothing");
    ASSERT_EQ(footCtrl.sentMessages.size(), 0);
    PASS();

    TEST("foot controller CC went to channel 16");
    // Find the Bank Select CC in synth messages (was CC 0)
    bool found = false;
    for (auto& msg : synth.sentMessages) {
        if (msg.size() >= 3 && (msg[0] & 0xF0) == 0xB0 && msg[1] == 0) {
            // Channel should be 16 = 0xBF
            ASSERT_EQ(msg[0], 0xBF);
            found = true;
            break;
        }
    }
    ASSERT(found);
    PASS();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    printf("ESP32_Host_MIDI — MIDIHandler test suite (v5.2 + routing)\n");
    printf("==========================================================\n");

    test_noteon_fields();
    test_noteoff_fields();
    test_noteon_vel0_is_noteoff();
    test_cc_fields();
    test_pc_fields();
    test_cp_fields();
    test_pb_fields();
    test_static_helpers();
    test_all_channels();
    test_velocity_scaling();
    test_queue();
    test_active_notes();
    test_chord_detection();
    test_velocity_threshold();
    test_event_metadata();
    test_raw_midi_format();
    test_edge_cases();

    // New routing tests
    test_source_tracking();
    test_targeted_send();
    test_transport_access();
    test_routing_scenario();

    printf("\n==========================================================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
