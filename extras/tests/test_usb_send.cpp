// ESP32_Host_MIDI — USB MIDI Send unit tests
// Tests CIN calculation and packet formatting for all MIDI message types.
// USB-MIDI 1.0 spec Table 4-1.
//
// Build:
//   g++ -std=c++11 -Wall -Wextra -Wno-unused-parameter -o extras/tests/test_usb_send extras/tests/test_usb_send.cpp

#include <cstdio>
#include <cstdint>
#include <cstring>

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
    do { printf("FAIL  %s\n", msg); ++g_fail; return; } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { \
        printf("FAIL  expected 0x%02X, got 0x%02X\n", (unsigned)(b), (unsigned)(a)); \
        ++g_fail; return; } } while(0)

// ---------------------------------------------------------------------------
// Extract _midiStatusToCIN from USBConnection.cpp (copy for testability)
// ---------------------------------------------------------------------------

static uint8_t _midiStatusToCIN(uint8_t status) {
    if (status >= 0x80 && status <= 0xEF) {
        return status >> 4;
    }
    switch (status) {
        case 0xF0: return 0x04; // SysEx Start
        case 0xF1: return 0x02; // MTC Quarter Frame (2 bytes)
        case 0xF2: return 0x03; // Song Position Pointer (3 bytes)
        case 0xF3: return 0x02; // Song Select (2 bytes)
        case 0xF6: return 0x05; // Tune Request (1 byte)
        case 0xF7: return 0x05; // SysEx End (single-byte)
        case 0xF8: return 0x0F; // Timing Clock
        case 0xFA: return 0x0F; // Start
        case 0xFB: return 0x0F; // Continue
        case 0xFC: return 0x0F; // Stop
        case 0xFE: return 0x0F; // Active Sensing
        case 0xFF: return 0x0F; // System Reset
        default:   return 0;    // Undefined
    }
}

// Builds a 4-byte USB-MIDI packet (same logic as USBConnection::sendMidiMessage)
static bool buildPacket(const uint8_t* data, size_t length, uint8_t* packet) {
    if (length == 0) return false;
    uint8_t cin = _midiStatusToCIN(data[0]);
    if (cin == 0) return false;
    packet[0] = cin;  // cable 0 | CIN
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0;
    for (size_t i = 0; i < length && i < 3; i++) {
        packet[i + 1] = data[i];
    }
    return true;
}

// ---------------------------------------------------------------------------
// CIN tests: Channel Voice Messages (0x8n - 0xEn)
// ---------------------------------------------------------------------------

void test_cin_note_off() {
    TEST("CIN NoteOff (0x8n)");
    for (uint8_t ch = 0; ch < 16; ch++) {
        ASSERT_EQ(_midiStatusToCIN(0x80 | ch), 0x08);
    }
    PASS();
}

void test_cin_note_on() {
    TEST("CIN NoteOn (0x9n)");
    for (uint8_t ch = 0; ch < 16; ch++) {
        ASSERT_EQ(_midiStatusToCIN(0x90 | ch), 0x09);
    }
    PASS();
}

void test_cin_poly_pressure() {
    TEST("CIN PolyPressure (0xAn)");
    ASSERT_EQ(_midiStatusToCIN(0xA0), 0x0A);
    ASSERT_EQ(_midiStatusToCIN(0xAF), 0x0A);
    PASS();
}

void test_cin_control_change() {
    TEST("CIN ControlChange (0xBn)");
    ASSERT_EQ(_midiStatusToCIN(0xB0), 0x0B);
    ASSERT_EQ(_midiStatusToCIN(0xBF), 0x0B);
    PASS();
}

void test_cin_program_change() {
    TEST("CIN ProgramChange (0xCn)");
    ASSERT_EQ(_midiStatusToCIN(0xC0), 0x0C);
    ASSERT_EQ(_midiStatusToCIN(0xCF), 0x0C);
    PASS();
}

void test_cin_channel_pressure() {
    TEST("CIN ChannelPressure (0xDn)");
    ASSERT_EQ(_midiStatusToCIN(0xD0), 0x0D);
    ASSERT_EQ(_midiStatusToCIN(0xDF), 0x0D);
    PASS();
}

void test_cin_pitch_bend() {
    TEST("CIN PitchBend (0xEn)");
    ASSERT_EQ(_midiStatusToCIN(0xE0), 0x0E);
    ASSERT_EQ(_midiStatusToCIN(0xEF), 0x0E);
    PASS();
}

// ---------------------------------------------------------------------------
// CIN tests: System Common Messages
// ---------------------------------------------------------------------------

void test_cin_sysex_start() {
    TEST("CIN SysEx Start (0xF0)");
    ASSERT_EQ(_midiStatusToCIN(0xF0), 0x04);
    PASS();
}

void test_cin_mtc_quarter_frame() {
    TEST("CIN MTC Quarter Frame (0xF1)");
    ASSERT_EQ(_midiStatusToCIN(0xF1), 0x02);
    PASS();
}

void test_cin_song_position() {
    TEST("CIN Song Position Pointer (0xF2)");
    ASSERT_EQ(_midiStatusToCIN(0xF2), 0x03);
    PASS();
}

void test_cin_song_select() {
    TEST("CIN Song Select (0xF3)");
    ASSERT_EQ(_midiStatusToCIN(0xF3), 0x02);
    PASS();
}

void test_cin_tune_request() {
    TEST("CIN Tune Request (0xF6)");
    ASSERT_EQ(_midiStatusToCIN(0xF6), 0x05);
    PASS();
}

void test_cin_sysex_end() {
    TEST("CIN SysEx End (0xF7)");
    ASSERT_EQ(_midiStatusToCIN(0xF7), 0x05);
    PASS();
}

// ---------------------------------------------------------------------------
// CIN tests: System Real-Time Messages
// ---------------------------------------------------------------------------

void test_cin_timing_clock() {
    TEST("CIN Timing Clock (0xF8)");
    ASSERT_EQ(_midiStatusToCIN(0xF8), 0x0F);
    PASS();
}

void test_cin_start() {
    TEST("CIN Start (0xFA)");
    ASSERT_EQ(_midiStatusToCIN(0xFA), 0x0F);
    PASS();
}

void test_cin_continue() {
    TEST("CIN Continue (0xFB)");
    ASSERT_EQ(_midiStatusToCIN(0xFB), 0x0F);
    PASS();
}

void test_cin_stop() {
    TEST("CIN Stop (0xFC)");
    ASSERT_EQ(_midiStatusToCIN(0xFC), 0x0F);
    PASS();
}

void test_cin_active_sensing() {
    TEST("CIN Active Sensing (0xFE)");
    ASSERT_EQ(_midiStatusToCIN(0xFE), 0x0F);
    PASS();
}

void test_cin_system_reset() {
    TEST("CIN System Reset (0xFF)");
    ASSERT_EQ(_midiStatusToCIN(0xFF), 0x0F);
    PASS();
}

// ---------------------------------------------------------------------------
// CIN tests: Undefined status bytes
// ---------------------------------------------------------------------------

void test_cin_undefined() {
    TEST("CIN undefined (0xF4, 0xF5, 0xFD)");
    ASSERT_EQ(_midiStatusToCIN(0xF4), 0x00);
    ASSERT_EQ(_midiStatusToCIN(0xF5), 0x00);
    ASSERT_EQ(_midiStatusToCIN(0xFD), 0x00);
    PASS();
}

void test_cin_not_status() {
    TEST("CIN rejects data bytes (0x00-0x7F)");
    ASSERT_EQ(_midiStatusToCIN(0x00), 0x00);
    ASSERT_EQ(_midiStatusToCIN(0x3C), 0x00);
    ASSERT_EQ(_midiStatusToCIN(0x7F), 0x00);
    PASS();
}

// ---------------------------------------------------------------------------
// Packet formatting tests
// ---------------------------------------------------------------------------

void test_packet_note_on() {
    TEST("Packet NoteOn ch1 C4 vel100");
    uint8_t data[] = { 0x90, 60, 100 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 3, pkt), true);
    ASSERT_EQ(pkt[0], 0x09);  // CIN
    ASSERT_EQ(pkt[1], 0x90);  // status
    ASSERT_EQ(pkt[2], 60);    // note
    ASSERT_EQ(pkt[3], 100);   // velocity
    PASS();
}

void test_packet_note_off() {
    TEST("Packet NoteOff ch1 C4");
    uint8_t data[] = { 0x80, 60, 0 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 3, pkt), true);
    ASSERT_EQ(pkt[0], 0x08);
    ASSERT_EQ(pkt[1], 0x80);
    ASSERT_EQ(pkt[2], 60);
    ASSERT_EQ(pkt[3], 0);
    PASS();
}

void test_packet_cc() {
    TEST("Packet CC ch1 sustain on");
    uint8_t data[] = { 0xB0, 64, 127 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 3, pkt), true);
    ASSERT_EQ(pkt[0], 0x0B);
    ASSERT_EQ(pkt[1], 0xB0);
    ASSERT_EQ(pkt[2], 64);
    ASSERT_EQ(pkt[3], 127);
    PASS();
}

void test_packet_program_change() {
    TEST("Packet ProgramChange ch1 prog5 (2 bytes)");
    uint8_t data[] = { 0xC0, 5 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 2, pkt), true);
    ASSERT_EQ(pkt[0], 0x0C);
    ASSERT_EQ(pkt[1], 0xC0);
    ASSERT_EQ(pkt[2], 5);
    ASSERT_EQ(pkt[3], 0);    // padded with zero
    PASS();
}

void test_packet_channel_pressure() {
    TEST("Packet ChannelPressure ch1 (2 bytes)");
    uint8_t data[] = { 0xD0, 100 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 2, pkt), true);
    ASSERT_EQ(pkt[0], 0x0D);
    ASSERT_EQ(pkt[1], 0xD0);
    ASSERT_EQ(pkt[2], 100);
    ASSERT_EQ(pkt[3], 0);
    PASS();
}

void test_packet_pitch_bend() {
    TEST("Packet PitchBend ch1 center");
    uint8_t data[] = { 0xE0, 0x00, 0x40 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 3, pkt), true);
    ASSERT_EQ(pkt[0], 0x0E);
    ASSERT_EQ(pkt[1], 0xE0);
    ASSERT_EQ(pkt[2], 0x00);
    ASSERT_EQ(pkt[3], 0x40);
    PASS();
}

void test_packet_mtc_quarter_frame() {
    TEST("Packet MTC Quarter Frame (2 bytes)");
    uint8_t data[] = { 0xF1, 0x23 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 2, pkt), true);
    ASSERT_EQ(pkt[0], 0x02);
    ASSERT_EQ(pkt[1], 0xF1);
    ASSERT_EQ(pkt[2], 0x23);
    ASSERT_EQ(pkt[3], 0);
    PASS();
}

void test_packet_song_position() {
    TEST("Packet Song Position Pointer (3 bytes)");
    uint8_t data[] = { 0xF2, 0x10, 0x20 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 3, pkt), true);
    ASSERT_EQ(pkt[0], 0x03);
    ASSERT_EQ(pkt[1], 0xF2);
    ASSERT_EQ(pkt[2], 0x10);
    ASSERT_EQ(pkt[3], 0x20);
    PASS();
}

void test_packet_song_select() {
    TEST("Packet Song Select (2 bytes)");
    uint8_t data[] = { 0xF3, 0x05 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 2, pkt), true);
    ASSERT_EQ(pkt[0], 0x02);
    ASSERT_EQ(pkt[1], 0xF3);
    ASSERT_EQ(pkt[2], 0x05);
    ASSERT_EQ(pkt[3], 0);
    PASS();
}

void test_packet_tune_request() {
    TEST("Packet Tune Request (1 byte)");
    uint8_t data[] = { 0xF6 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), true);
    ASSERT_EQ(pkt[0], 0x05);
    ASSERT_EQ(pkt[1], 0xF6);
    ASSERT_EQ(pkt[2], 0);
    ASSERT_EQ(pkt[3], 0);
    PASS();
}

void test_packet_realtime_clock() {
    TEST("Packet Timing Clock (1 byte)");
    uint8_t data[] = { 0xF8 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), true);
    ASSERT_EQ(pkt[0], 0x0F);
    ASSERT_EQ(pkt[1], 0xF8);
    ASSERT_EQ(pkt[2], 0);
    ASSERT_EQ(pkt[3], 0);
    PASS();
}

void test_packet_realtime_start() {
    TEST("Packet Start (1 byte)");
    uint8_t data[] = { 0xFA };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), true);
    ASSERT_EQ(pkt[0], 0x0F);
    ASSERT_EQ(pkt[1], 0xFA);
    PASS();
}

void test_packet_realtime_stop() {
    TEST("Packet Stop (1 byte)");
    uint8_t data[] = { 0xFC };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), true);
    ASSERT_EQ(pkt[0], 0x0F);
    ASSERT_EQ(pkt[1], 0xFC);
    PASS();
}

void test_packet_active_sensing() {
    TEST("Packet Active Sensing (1 byte)");
    uint8_t data[] = { 0xFE };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), true);
    ASSERT_EQ(pkt[0], 0x0F);
    ASSERT_EQ(pkt[1], 0xFE);
    PASS();
}

void test_packet_system_reset() {
    TEST("Packet System Reset (1 byte)");
    uint8_t data[] = { 0xFF };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), true);
    ASSERT_EQ(pkt[0], 0x0F);
    ASSERT_EQ(pkt[1], 0xFF);
    PASS();
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

void test_packet_rejects_undefined() {
    TEST("Packet rejects undefined status 0xF4");
    uint8_t data[] = { 0xF4 };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), false);
    PASS();
}

void test_packet_rejects_data_byte() {
    TEST("Packet rejects data byte 0x3C");
    uint8_t data[] = { 0x3C };
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(data, 1, pkt), false);
    PASS();
}

void test_packet_rejects_zero_length() {
    TEST("Packet rejects zero length");
    uint8_t pkt[4];
    ASSERT_EQ(buildPacket(nullptr, 0, pkt), false);
    PASS();
}

void test_packet_all_channels_note_on() {
    TEST("Packet NoteOn all 16 channels");
    uint8_t pkt[4];
    for (uint8_t ch = 0; ch < 16; ch++) {
        uint8_t data[] = { (uint8_t)(0x90 | ch), 60, 100 };
        ASSERT_EQ(buildPacket(data, 3, pkt), true);
        ASSERT_EQ(pkt[0], 0x09);
        ASSERT_EQ(pkt[1], (uint8_t)(0x90 | ch));
    }
    PASS();
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    printf("=== USB MIDI Send Tests ===\n\n");

    printf("[CIN: Channel Voice]\n");
    test_cin_note_off();
    test_cin_note_on();
    test_cin_poly_pressure();
    test_cin_control_change();
    test_cin_program_change();
    test_cin_channel_pressure();
    test_cin_pitch_bend();

    printf("\n[CIN: System Common]\n");
    test_cin_sysex_start();
    test_cin_mtc_quarter_frame();
    test_cin_song_position();
    test_cin_song_select();
    test_cin_tune_request();
    test_cin_sysex_end();

    printf("\n[CIN: System Real-Time]\n");
    test_cin_timing_clock();
    test_cin_start();
    test_cin_continue();
    test_cin_stop();
    test_cin_active_sensing();
    test_cin_system_reset();

    printf("\n[CIN: Edge Cases]\n");
    test_cin_undefined();
    test_cin_not_status();

    printf("\n[Packet: Channel Voice]\n");
    test_packet_note_on();
    test_packet_note_off();
    test_packet_cc();
    test_packet_program_change();
    test_packet_channel_pressure();
    test_packet_pitch_bend();

    printf("\n[Packet: System Common]\n");
    test_packet_mtc_quarter_frame();
    test_packet_song_position();
    test_packet_song_select();
    test_packet_tune_request();

    printf("\n[Packet: System Real-Time]\n");
    test_packet_realtime_clock();
    test_packet_realtime_start();
    test_packet_realtime_stop();
    test_packet_active_sensing();
    test_packet_system_reset();

    printf("\n[Edge Cases]\n");
    test_packet_rejects_undefined();
    test_packet_rejects_data_byte();
    test_packet_rejects_zero_length();
    test_packet_all_channels_note_on();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
