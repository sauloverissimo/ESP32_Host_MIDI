// ESP32_Host_MIDI — native test suite
// Covers platform-agnostic core: MIDITransport, MIDIHandlerConfig, MIDI2Support.
// Build:
//   g++ -std=c++11 -Iextras/tests/stub -Isrc -Wall -Wextra -Wno-unused-parameter \
//       -o extras/tests/test_native extras/tests/test_native.cpp

#include <cstdio>
#include <cstring>

// Stub Arduino.h must come before MIDI2Support.h
#include "stub/Arduino.h"

#include "../../src/MIDITransport.h"
#include "../../src/MIDIHandlerConfig.h"
#include "../../src/MIDI2Support.h"

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-52s", name); } while(0)

#define PASS() \
    do { printf("OK\n"); ++g_pass; } while(0)

#define FAIL(msg) \
    do { printf("FAIL — %s\n", msg); ++g_fail; return; } while(0)

#define ASSERT(expr) \
    do { if (!(expr)) { printf("FAIL — " #expr "\n"); ++g_fail; return; } } while(0)

// ---------------------------------------------------------------------------
// MIDITransport — virtual dispatch via concrete mock
// ---------------------------------------------------------------------------

struct MockTransport : public MIDITransport {
    bool connected = false;
    bool taskCalled = false;
    void task() override { taskCalled = true; }
    bool isConnected() const override { return connected; }
};

static bool s_cbFired = false;
static void testCb(void*, const uint8_t*, size_t) { s_cbFired = true; }

void test_transport() {
    printf("\n[MIDITransport]\n");

    TEST("concrete subclass — isConnected true/false");
    MockTransport t;
    ASSERT(!t.isConnected());
    t.connected = true;
    ASSERT(t.isConnected());
    PASS();

    TEST("task() is called");
    t.task();
    ASSERT(t.taskCalled);
    PASS();

    TEST("MIDI data callback fires via dispatchMidiData");
    s_cbFired = false;
    t.setMidiCallback(testCb, nullptr);
    const uint8_t midi[3] = {0x90, 60, 100};
    // dispatchMidiData is protected — access via a helper subclass
    struct TestableTransport : public MockTransport {
        void fireDispatch(const uint8_t* d, size_t l) { dispatchMidiData(d, l); }
    } tt;
    tt.setMidiCallback(testCb, nullptr);
    tt.fireDispatch(midi, 3);
    ASSERT(s_cbFired);
    PASS();
}

// ---------------------------------------------------------------------------
// MIDIHandlerConfig — default values
// ---------------------------------------------------------------------------

void test_config() {
    printf("\n[MIDIHandlerConfig]\n");

    TEST("default maxEvents");
    MIDIHandlerConfig cfg;
    ASSERT(cfg.maxEvents == 20);
    PASS();

    TEST("default chordTimeWindow");
    ASSERT(cfg.chordTimeWindow == 0);
    PASS();

    TEST("default velocityThreshold");
    ASSERT(cfg.velocityThreshold == 0);
    PASS();

    TEST("default maxSysExSize");
    ASSERT(cfg.maxSysExSize == 512);
    PASS();

    TEST("default maxSysExEvents");
    ASSERT(cfg.maxSysExEvents == 8);
    PASS();

    TEST("custom values preserved");
    cfg.maxEvents = 50;
    cfg.chordTimeWindow = 60;
    ASSERT(cfg.maxEvents == 50 && cfg.chordTimeWindow == 60);
    PASS();
}

// ---------------------------------------------------------------------------
// MIDI2Scaler — boundary values and roundtrips
// ---------------------------------------------------------------------------

void test_scaler() {
    printf("\n[MIDI2Scaler]\n");

    TEST("scale7to16 — 0 maps to 0x0000");
    ASSERT(MIDI2Scaler::scale7to16(0) == 0x0000);
    PASS();

    TEST("scale7to16 — 127 maps to 0xFFFF");
    ASSERT(MIDI2Scaler::scale7to16(127) == 0xFFFF);
    PASS();

    TEST("scale7to32 — 0 maps to 0x00000000");
    ASSERT(MIDI2Scaler::scale7to32(0) == 0x00000000U);
    PASS();

    TEST("scale7to32 — 127 maps to 0xFFFFFFFF");
    ASSERT(MIDI2Scaler::scale7to32(127) == 0xFFFFFFFFU);
    PASS();

    TEST("scale16to7 — roundtrip boundaries");
    ASSERT(MIDI2Scaler::scale16to7(0x0000) == 0);
    ASSERT(MIDI2Scaler::scale16to7(0xFFFF) == 127);
    PASS();

    TEST("scale32to7 — roundtrip boundaries");
    ASSERT(MIDI2Scaler::scale32to7(0x00000000U) == 0);
    ASSERT(MIDI2Scaler::scale32to7(0xFFFFFFFFU) == 127);
    PASS();

    TEST("scale7to16 → scale16to7 mid-range");
    uint16_t v16 = MIDI2Scaler::scale7to16(64);
    ASSERT(MIDI2Scaler::scale16to7(v16) == 64);
    PASS();

    TEST("scale7to32 → scale32to7 mid-range");
    uint32_t v32 = MIDI2Scaler::scale7to32(64);
    ASSERT(MIDI2Scaler::scale32to7(v32) == 64);
    PASS();

    TEST("pitchBend14to32 — center (0) near 0x80000000");
    uint32_t center = MIDI2Scaler::pitchBend14to32(0);
    ASSERT(center >= 0x7F000000U && center <= 0x81000000U);
    PASS();

    TEST("pitchBend14to32 → pitchBend32to14 roundtrip at center");
    int pb14 = MIDI2Scaler::pitchBend32to14(MIDI2Scaler::pitchBend14to32(0));
    ASSERT(pb14 >= -1 && pb14 <= 1);  // rounding tolerance
    PASS();
}

// ---------------------------------------------------------------------------
// UMPWord32 — field extraction
// ---------------------------------------------------------------------------

void test_ump32() {
    printf("\n[UMPWord32]\n");

    // MT=2 (MIDI1 voice), group=0, status=0x90 (NoteOn ch0), note=60, vel=100
    // raw = 0x2090_3C64
    TEST("field extraction — NoteOn ch0 note=60 vel=100");
    UMPWord32 w(0x20903C64U);
    ASSERT(w.messageType() == 0x2);
    ASSERT(w.group()       == 0x0);
    ASSERT(w.statusByte()  == 0x90);
    ASSERT(w.data1()       == 60);
    ASSERT(w.data2()       == 100);
    PASS();

    TEST("default constructor gives zero raw");
    UMPWord32 zero;
    ASSERT(zero.raw == 0);
    PASS();
}

// ---------------------------------------------------------------------------
// UMPWord64 — field extraction
// ---------------------------------------------------------------------------

void test_ump64() {
    printf("\n[UMPWord64]\n");

    // MT=4, group=0, opcode=9 (NoteOn), ch=0, note=60, optionFlags=0
    // word0 = 0x4090_3C00, word1 = 0xFFFF_0000 (velocity=0xFFFF, attr=0)
    TEST("field extraction — MIDI2 NoteOn ch0 note=60 vel=0xFFFF");
    UMPWord64 w(0x40903C00U, 0xFFFF0000U);
    ASSERT(w.messageType() == 0x4);
    ASSERT(w.group()       == 0x0);
    ASSERT(w.opcode()      == (uint8_t)MIDI2_OP_NOTE_ON);
    ASSERT(w.channel()     == 0x0);
    ASSERT(w.index()       == 60);
    ASSERT(w.dataHi()      == 0xFFFF);
    ASSERT(w.dataLo()      == 0x0000);
    PASS();

    TEST("default constructor gives zero words");
    UMPWord64 zero;
    ASSERT(zero.word0 == 0 && zero.word1 == 0);
    PASS();
}

// ---------------------------------------------------------------------------
// UMPBuilder — packet construction
// ---------------------------------------------------------------------------

void test_builder() {
    printf("\n[UMPBuilder]\n");

    TEST("fromMIDI1 — NoteOn ch0 note=60 vel=100");
    const uint8_t raw[3] = {0x90, 60, 100};
    UMPWord32 w = UMPBuilder::fromMIDI1(0, raw, 3);
    ASSERT(w.messageType() == UMP_MT_MIDI1_VOICE);
    ASSERT(w.group()       == 0);
    ASSERT(w.statusByte()  == 0x90);
    ASSERT(w.data1()       == 60);
    ASSERT(w.data2()       == 100);
    PASS();

    TEST("noteOn — MIDI2 ch0 note=60 vel=0xFFFF");
    UMPWord64 on = UMPBuilder::noteOn(0, 0, 60, 0xFFFF);
    ASSERT(on.messageType() == UMP_MT_MIDI2_VOICE);
    ASSERT(on.opcode()      == (uint8_t)MIDI2_OP_NOTE_ON);
    ASSERT(on.channel()     == 0);
    ASSERT(on.index()       == 60);
    ASSERT(on.dataHi()      == 0xFFFF);
    PASS();

    TEST("noteOff — MIDI2 ch1 note=64 vel=0");
    UMPWord64 off = UMPBuilder::noteOff(0, 1, 64, 0);
    ASSERT(off.messageType() == UMP_MT_MIDI2_VOICE);
    ASSERT(off.opcode()      == (uint8_t)MIDI2_OP_NOTE_OFF);
    ASSERT(off.channel()     == 1);
    ASSERT(off.index()       == 64);
    PASS();

    TEST("controlChange — CC7 (volume) value=0xFFFFFFFF");
    UMPWord64 cc = UMPBuilder::controlChange(0, 0, 7, 0xFFFFFFFFU);
    ASSERT(cc.opcode() == (uint8_t)MIDI2_OP_CONTROL_CHANGE);
    ASSERT(cc.index()  == 7);
    ASSERT(cc.data()   == 0xFFFFFFFFU);
    PASS();

    TEST("sysEx7 — complete 3-byte payload");
    const uint8_t payload[3] = {0x41, 0x10, 0x42};
    UMPWord64 sx = UMPBuilder::sysEx7(0, SYSEX7_COMPLETE, payload, 3);
    ASSERT(((sx.word0 >> 28) & 0x0F) == UMP_MT_DATA_64);
    ASSERT(((sx.word0 >> 20) & 0x0F) == SYSEX7_COMPLETE);
    ASSERT(((sx.word0 >> 16) & 0x0F) == 3);  // byte count
    ASSERT(((sx.word0 >>  8) & 0xFF) == 0x41);
    ASSERT(((sx.word0 >>  0) & 0xFF) == 0x10);
    ASSERT(((sx.word1 >> 24) & 0xFF) == 0x42);
    PASS();
}

// ---------------------------------------------------------------------------
// UMPParser — parse MIDI1-in-UMP and MIDI2 packets
// ---------------------------------------------------------------------------

void test_parser() {
    printf("\n[UMPParser]\n");

    TEST("parseMIDI1 — NoteOn ch0 note=60 vel=100");
    UMPWord32 w32(0x20903C64U);
    UMPResult r = UMPParser::parseMIDI1(w32);
    ASSERT(r.valid      == true);
    ASSERT(r.isMIDI2    == false);
    ASSERT(r.group      == 0);
    ASSERT(r.channel    == 0);     // status 0x90 & 0x0F = 0
    ASSERT(r.opcode     == 9);     // (0x90 >> 4) & 0x0F
    ASSERT(r.note       == 60);
    ASSERT(r.midi1[0]   == 0x90);
    ASSERT(r.midi1[1]   == 60);
    ASSERT(r.midi1[2]   == 100);
    PASS();

    TEST("parseMIDI1 — rejects non-MIDI1 packet");
    UMPWord32 bad(0x40903C00U);  // MT=4, not MT=2
    UMPResult rb = UMPParser::parseMIDI1(bad);
    ASSERT(rb.valid == false);
    PASS();

    TEST("parseMIDI2 — NoteOn ch0 note=60 vel=0xFFFF");
    UMPWord64 w64(0x40903C00U, 0xFFFF0000U);
    UMPResult r2 = UMPParser::parseMIDI2(w64);
    ASSERT(r2.valid    == true);
    ASSERT(r2.isMIDI2  == true);
    ASSERT(r2.opcode   == (uint8_t)MIDI2_OP_NOTE_ON);
    ASSERT(r2.channel  == 0);
    ASSERT(r2.note     == 60);
    ASSERT(r2.midi1[1] == 60);
    ASSERT(r2.midi1[2] == 127);  // scale16to7(0xFFFF) == 127
    PASS();

    TEST("parseMIDI2 — rejects non-MIDI2 packet");
    UMPWord64 bad2(0x20903C00U, 0);  // MT=2, not MT=4
    UMPResult rb2 = UMPParser::parseMIDI2(bad2);
    ASSERT(rb2.valid == false);
    PASS();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    printf("ESP32_Host_MIDI — native test suite\n");
    printf("====================================\n");

    test_transport();
    test_config();
    test_scaler();
    test_ump32();
    test_ump64();
    test_builder();
    test_parser();

    printf("\n====================================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
