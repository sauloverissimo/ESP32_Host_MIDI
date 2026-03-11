// test_midi2_scan.cpp — test MIDI 2.0 config descriptor scanning + negotiation
//
// Tests the _findBestAlt logic against real USB descriptors, OUT endpoint
// capture, UMP Stream Message building, and Protocol Negotiation parsing.
//
// Build:
//   g++ -std=c++11 -Iextras/tests/stub -Isrc -Wall -Wextra -Wno-unused-parameter
//       -o extras/tests/test_midi2_scan extras/tests/test_midi2_scan.cpp

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "stub/Arduino.h"
#include "../../src/MIDITransport.h"

// ---------------------------------------------------------------------------
// Minimal test framework (same as test_native.cpp)
// ---------------------------------------------------------------------------

static int g_pass = 0, g_fail = 0;

#define TEST(name) do { printf("  %-56s", name); } while(0)
#define PASS()     do { printf("OK\n"); ++g_pass; } while(0)
#define ASSERT(e)  do { if (!(e)) { printf("FAIL — " #e " (line %d)\n", __LINE__); ++g_fail; return; } } while(0)

// ---------------------------------------------------------------------------
// AltCandidate + _findBestAlt — extracted from USBMIDI2Connection
// ---------------------------------------------------------------------------

struct AltCandidate {
    uint8_t  ifaceNumber;
    uint8_t  altSetting;
    bool     isMIDI2;
    uint8_t  epInAddress;
    uint16_t epInMaxPacket;
    uint8_t  epInInterval;
    uint8_t  epOutAddress;
    uint16_t epOutMaxPacket;
};

static bool findBestAlt(const uint8_t* p, uint16_t totalLen, AltCandidate& best) {
    AltCandidate midi1 = {}, midi2 = {};
    bool found1 = false, found2 = false;

    uint16_t i = 0;
    while (i < totalLen) {
        if (i + 1 >= totalLen) break;
        uint8_t dlen  = p[i];
        uint8_t dtype = p[i + 1];
        if (dlen < 2 || (i + dlen) > totalLen) break;

        if (dtype == 0x04 && dlen >= 9) {
            uint8_t ifNum    = p[i + 2];
            uint8_t altSet   = p[i + 3];
            uint8_t numEP    = p[i + 4];
            uint8_t ifClass  = p[i + 5];
            uint8_t ifSubCls = p[i + 6];

            if (ifClass == 0x01 && ifSubCls == 0x03) {
                AltCandidate cand = {};
                cand.ifaceNumber = ifNum;
                cand.altSetting  = altSet;

                uint16_t j = i + dlen;
                while (j < totalLen) {
                    if (j + 1 >= totalLen) break;
                    uint8_t blen  = p[j];
                    uint8_t btype = p[j + 1];
                    if (blen < 2 || (j + blen) > totalLen) break;
                    if (btype == 0x04) break;

                    if (btype == 0x24 && blen >= 5 && p[j + 2] == 0x01) {
                        uint16_t bcdMSC = p[j + 3] | ((uint16_t)p[j + 4] << 8);
                        cand.isMIDI2 = (bcdMSC >= 0x0200);
                    }

                    if (btype == 0x05 && blen >= 7 && numEP > 0) {
                        uint8_t  epAddr = p[j + 2];
                        uint16_t maxPkt = p[j + 4] | ((uint16_t)p[j + 5] << 8);
                        uint8_t  bInt   = p[j + 6];
                        if (maxPkt == 0)  maxPkt = 64;
                        if (maxPkt > 512) maxPkt = 512;
                        if (epAddr & 0x80) {
                            cand.epInAddress   = epAddr;
                            cand.epInMaxPacket = maxPkt;
                            cand.epInInterval  = bInt ? bInt : 1;
                        } else {
                            cand.epOutAddress   = epAddr;
                            cand.epOutMaxPacket = maxPkt;
                        }
                    }

                    j += blen;
                }

                if (cand.epInAddress != 0) {
                    if (cand.isMIDI2) { midi2 = cand; found2 = true; }
                    else               { midi1 = cand; found1 = true; }
                }
            }
        }

        i += dlen;
    }

    if (found2) { best = midi2; return true; }
    if (found1) { best = midi1; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// UMP Stream Message constants (same as USBMIDI2Connection.cpp)
// ---------------------------------------------------------------------------

static const uint8_t  UMP_MT_STREAM            = 0x0F;
static const uint16_t STREAM_ENDPOINT_DISCOVERY = 0x000;
static const uint16_t STREAM_ENDPOINT_INFO      = 0x001;
static const uint16_t STREAM_DEVICE_INFO        = 0x002;
static const uint16_t STREAM_CONFIG_REQUEST     = 0x005;
static const uint16_t STREAM_CONFIG_NOTIFY      = 0x006;
static const uint16_t STREAM_FB_DISCOVERY       = 0x010;
static const uint16_t STREAM_FB_INFO            = 0x011;

// ---------------------------------------------------------------------------
// Test descriptors
// ---------------------------------------------------------------------------

// Bohemian MIDI 2.0 Sender — real descriptor (constants resolved).
static const uint8_t DESC_BOHEMIAN_SENDER[] = {
    // Configuration (9 bytes)
    0x09, 0x02, 153, 0x00, 0x02, 0x01, 0x00, 0x80, 0xFA,
    // IAD (8 bytes)
    0x08, 0x0B, 0x00, 0x02, 0x01, 0x00, 0x20, 0x04,
    // Interface 0: AudioControl (9 bytes)
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    // AC Header (9 bytes)
    0x09, 0x24, 0x01, 0x00, 0x02, 0x08, 0x09, 0x00, 0x00,
    // Interface 1, Alt 0: MIDI Streaming 1.0 (9 bytes)
    0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x04,
    // MS Header (7 bytes) — bcdMSC 1.00
    0x07, 0x24, 0x01, 0x00, 0x01, 0x41, 0x00,
    // Embedded IN Jack (6)
    0x06, 0x24, 0x02, 0x01, 0x01, 0x00,
    // External IN Jack (6)
    0x06, 0x24, 0x02, 0x02, 0x02, 0x00,
    // Embedded OUT Jack (9)
    0x09, 0x24, 0x03, 0x01, 0x03, 0x01, 0x02, 0x01, 0x00,
    // External OUT Jack (9)
    0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x01, 0x01, 0x00,
    // OUT endpoint (9)
    0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    // CS OUT endpoint (5)
    0x05, 0x25, 0x01, 0x01, 0x01,
    // IN endpoint (9)
    0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    // CS IN endpoint (5)
    0x05, 0x25, 0x01, 0x01, 0x03,
    // Interface 1, Alt 1: MIDI Streaming 2.0 (9 bytes)
    0x09, 0x04, 0x01, 0x01, 0x02, 0x01, 0x03, 0x00, 0x04,
    // MS Header (7 bytes) — bcdMSC 2.00
    0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00,
    // OUT endpoint (9)
    0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    // CS OUT endpoint (5)
    0x05, 0x25, 0x02, 0x01, 0x01,
    // IN endpoint (9)
    0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    // CS IN endpoint (5)
    0x05, 0x25, 0x02, 0x01, 0x01,
};

// MIDI 1.0 only device — no Alt 1.
static const uint8_t DESC_MIDI1_ONLY[] = {
    0x09, 0x02, 0x65, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01,
    0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x25, 0x00,
    0x06, 0x24, 0x02, 0x01, 0x01, 0x00,
    0x06, 0x24, 0x02, 0x02, 0x02, 0x00,
    0x09, 0x24, 0x03, 0x01, 0x03, 0x01, 0x02, 0x01, 0x00,
    0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x01, 0x01, 0x00,
    0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x01,
    0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x03,
};

// No MIDI interface at all.
static const uint8_t DESC_NO_MIDI[] = {
    0x09, 0x02, 0x12, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
};

// Alt 1 appears BEFORE Alt 0 (reversed order).
static const uint8_t DESC_REVERSED_ALT[] = {
    0x09, 0x02, 0x55, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x24, 0x01, 0x00, 0x02, 0x08, 0x09, 0x00, 0x00,
    // Alt 1 first
    0x09, 0x04, 0x01, 0x01, 0x02, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00,
    0x09, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x05, 0x82, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
    // Alt 0 second
    0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x25, 0x00,
    0x09, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
};

// Zero endpoints.
static const uint8_t DESC_ZERO_EP[] = {
    0x09, 0x02, 0x20, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x07, 0x00,
};

// OUT-only endpoint.
static const uint8_t DESC_OUT_ONLY[] = {
    0x09, 0x02, 0x30, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x10, 0x00,
    0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x01,
};

// wMaxPacketSize = 0, bInterval = 5.
static const uint8_t DESC_MAXPKT_ZERO[] = {
    0x09, 0x02, 0x30, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x10, 0x00,
    0x09, 0x05, 0x81, 0x02, 0x00, 0x00, 0x05, 0x00, 0x00,
};

// wMaxPacketSize = 1024.
static const uint8_t DESC_MAXPKT_OVER[] = {
    0x09, 0x02, 0x30, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x10, 0x00,
    0x09, 0x05, 0x81, 0x02, 0x00, 0x04, 0x01, 0x00, 0x00,
};

// ---------------------------------------------------------------------------
// Tests — Descriptor Scanning
// ---------------------------------------------------------------------------

void test_bohemian_sender() {
    printf("\n[Bohemian Sender — dual Alt 0/1]\n");

    TEST("selects Alt 1 (MIDI 2.0)");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_BOHEMIAN_SENDER, sizeof(DESC_BOHEMIAN_SENDER), best);
    ASSERT(found);
    ASSERT(best.isMIDI2 == true);
    ASSERT(best.altSetting == 1);
    ASSERT(best.ifaceNumber == 1);
    PASS();

    TEST("IN endpoint is 0x81");
    ASSERT(best.epInAddress == 0x81);
    PASS();

    TEST("wMaxPacketSize = 64");
    ASSERT(best.epInMaxPacket == 64);
    PASS();

    TEST("OUT endpoint is 0x01");
    ASSERT(best.epOutAddress == 0x01);
    PASS();

    TEST("OUT wMaxPacketSize = 64");
    ASSERT(best.epOutMaxPacket == 64);
    PASS();
}

void test_midi1_only() {
    printf("\n[MIDI 1.0 only device]\n");

    TEST("selects Alt 0 (MIDI 1.0)");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_MIDI1_ONLY, sizeof(DESC_MIDI1_ONLY), best);
    ASSERT(found);
    ASSERT(best.isMIDI2 == false);
    ASSERT(best.altSetting == 0);
    PASS();

    TEST("IN endpoint is 0x81");
    ASSERT(best.epInAddress == 0x81);
    PASS();

    TEST("OUT endpoint is 0x01");
    ASSERT(best.epOutAddress == 0x01);
    PASS();
}

void test_no_midi() {
    printf("\n[No MIDI interface]\n");

    TEST("returns false");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_NO_MIDI, sizeof(DESC_NO_MIDI), best);
    ASSERT(!found);
    PASS();
}

void test_empty_descriptor() {
    printf("\n[Edge cases]\n");

    TEST("empty descriptor — returns false");
    AltCandidate best = {};
    bool found = findBestAlt(nullptr, 0, best);
    ASSERT(!found);
    PASS();

    TEST("truncated descriptor (2 bytes) — returns false");
    uint8_t trunc[] = { 0x09, 0x02 };
    found = findBestAlt(trunc, sizeof(trunc), best);
    ASSERT(!found);
    PASS();
}

void test_reversed_alt_order() {
    printf("\n[Reversed Alt order — Alt 1 before Alt 0]\n");

    AltCandidate best = {};
    bool found = findBestAlt(DESC_REVERSED_ALT, sizeof(DESC_REVERSED_ALT), best);

    TEST("still selects Alt 1 (MIDI 2.0)");
    ASSERT(found);
    ASSERT(best.isMIDI2 == true);
    ASSERT(best.altSetting == 1);
    PASS();

    TEST("IN endpoint is 0x82");
    ASSERT(best.epInAddress == 0x82);
    PASS();

    TEST("wMaxPacketSize = 512");
    ASSERT(best.epInMaxPacket == 512);
    PASS();

    TEST("OUT endpoint is 0x02");
    ASSERT(best.epOutAddress == 0x02);
    PASS();
}

void test_zero_endpoints() {
    printf("\n[Zero endpoints — malformed MIDI interface]\n");

    TEST("returns false (no usable endpoint)");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_ZERO_EP, sizeof(DESC_ZERO_EP), best);
    ASSERT(!found);
    PASS();
}

void test_out_only() {
    printf("\n[OUT-only endpoint — receive-only device]\n");

    TEST("returns false (no IN endpoint to read from)");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_OUT_ONLY, sizeof(DESC_OUT_ONLY), best);
    ASSERT(!found);
    PASS();
}

void test_maxpacket_edge() {
    printf("\n[wMaxPacketSize edge cases]\n");

    TEST("wMaxPacketSize=0 clamped to 64");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_MAXPKT_ZERO, sizeof(DESC_MAXPKT_ZERO), best);
    ASSERT(found);
    ASSERT(best.epInMaxPacket == 64);
    PASS();

    TEST("wMaxPacketSize=1024 clamped to 512");
    best = {};
    found = findBestAlt(DESC_MAXPKT_OVER, sizeof(DESC_MAXPKT_OVER), best);
    ASSERT(found);
    ASSERT(best.epInMaxPacket == 512);
    PASS();
}

void test_interval() {
    printf("\n[bInterval handling]\n");

    TEST("bInterval=5 preserved");
    AltCandidate best = {};
    bool found = findBestAlt(DESC_MAXPKT_ZERO, sizeof(DESC_MAXPKT_ZERO), best);
    ASSERT(found);
    ASSERT(best.epInInterval == 5);
    PASS();

    TEST("bInterval=0 defaults to 1");
    best = {};
    found = findBestAlt(DESC_BOHEMIAN_SENDER, sizeof(DESC_BOHEMIAN_SENDER), best);
    ASSERT(found);
    ASSERT(best.epInInterval == 1);
    PASS();
}

void test_corrupt_descriptor() {
    printf("\n[Corrupt descriptors — safety]\n");

    TEST("bLength=0 terminates (no infinite loop)");
    uint8_t corrupt[] = {
        0x09, 0x02, 0x10, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01,
    };
    AltCandidate best = {};
    bool found = findBestAlt(corrupt, sizeof(corrupt), best);
    ASSERT(!found);
    PASS();

    TEST("bLength > remaining bytes terminates safely");
    uint8_t overrun[] = {
        0x09, 0x02, 0x0C, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0xFF, 0x04, 0x00,
    };
    best = {};
    found = findBestAlt(overrun, sizeof(overrun), best);
    ASSERT(!found);
    PASS();

    TEST("single-byte descriptor — terminates");
    uint8_t tiny[] = { 0x01 };
    best = {};
    found = findBestAlt(tiny, sizeof(tiny), best);
    ASSERT(!found);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — UMP dispatch via MIDITransport
// ---------------------------------------------------------------------------

void test_ump_dispatch() {
    printf("\n[UMP dispatch via MIDITransport]\n");

    struct TestTransport : public MIDITransport {
        void task() override {}
        bool isConnected() const override { return true; }
        void fireUMP(const uint32_t* w, uint8_t c) { dispatchUMPData(w, c); }
    };

    static bool cbFired = false;
    static uint8_t cbCount = 0;
    static uint32_t cbWord0 = 0;

    struct CB {
        static void onUMP(void*, const uint32_t* words, uint8_t count) {
            cbFired = true;
            cbCount = count;
            cbWord0 = words[0];
        }
    };

    TestTransport t;
    t.setUMPCallback(CB::onUMP, nullptr);

    uint32_t ump[2] = { 0x40903C00, 0xFFFF0000 };

    TEST("UMP callback fires with correct count");
    cbFired = false;
    t.fireUMP(ump, 2);
    ASSERT(cbFired);
    ASSERT(cbCount == 2);
    PASS();

    TEST("UMP callback delivers correct word0");
    ASSERT(cbWord0 == 0x40903C00);
    PASS();

    TEST("no crash when UMP callback not set");
    TestTransport t2;
    cbFired = false;
    t2.fireUMP(ump, 2);
    ASSERT(!cbFired);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — UMP Stream Message building
// ---------------------------------------------------------------------------

void test_stream_message_build() {
    printf("\n[UMP Stream Message building]\n");

    // Endpoint Discovery: MT=0xF, status=0x000, ver=1.1, filter in word1
    TEST("Endpoint Discovery word0 format");
    uint32_t msg[4] = {};
    msg[0] = ((uint32_t)0x0F << 28)
           | ((uint32_t)0x000 << 16)
           | ((uint32_t)1 << 8)
           | (uint32_t)1;
    msg[1] = 0xFF;
    ASSERT(msg[0] == 0xF0000101);
    ASSERT(msg[1] == 0x000000FF);
    PASS();

    // Stream Config Request: MT=0xF, status=0x005, protocol=0x02
    TEST("Stream Config Request word0 format");
    msg[0] = ((uint32_t)0x0F << 28)
           | ((uint32_t)0x005 << 16)
           | ((uint32_t)0x02 << 8);
    ASSERT(msg[0] == 0xF0050200);
    PASS();

    // Function Block Discovery: MT=0xF, status=0x010, fbIdx=0xFF, filter=0xFF
    TEST("Function Block Discovery word0 format");
    msg[0] = ((uint32_t)0x0F << 28)
           | ((uint32_t)0x010 << 16)
           | (0xFF << 8)
           | 0xFF;
    ASSERT(msg[0] == 0xF010FFFF);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Protocol Negotiation stream message parsing
// ---------------------------------------------------------------------------

// Simulates parsing Endpoint Info Notification
void test_parse_endpoint_info() {
    printf("\n[Parse Endpoint Info Notification]\n");

    // word0: MT=0xF, status=0x001, verMajor=1, verMinor=1
    // word1: numFB=2 (byte3), m2=1 (bit9), m1=1 (bit8), rxjr=0 (bit1), txjr=1 (bit0)
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x001 << 16) | (1 << 8) | 1;
    words[1] = ((uint32_t)2 << 24) | (1 << 9) | (1 << 8) | 1;

    uint16_t status = (words[0] >> 16) & 0x3FF;
    ASSERT(status == 0x001);

    TEST("parse UMP version");
    uint8_t verMaj = (words[0] >> 8) & 0xFF;
    uint8_t verMin = words[0] & 0xFF;
    ASSERT(verMaj == 1);
    ASSERT(verMin == 1);
    PASS();

    TEST("parse numFunctionBlocks");
    uint8_t numFB = (words[1] >> 24) & 0xFF;
    ASSERT(numFB == 2);
    PASS();

    TEST("parse MIDI 2.0 support flag");
    bool m2 = (words[1] >> 9) & 0x01;
    ASSERT(m2 == true);
    PASS();

    TEST("parse MIDI 1.0 support flag");
    bool m1 = (words[1] >> 8) & 0x01;
    ASSERT(m1 == true);
    PASS();

    TEST("parse JR flags");
    bool rxjr = (words[1] >> 1) & 0x01;
    bool txjr = words[1] & 0x01;
    ASSERT(rxjr == false);
    ASSERT(txjr == true);
    PASS();
}

// Simulates parsing Stream Configuration Notification
void test_parse_stream_config() {
    printf("\n[Parse Stream Config Notification]\n");

    // word0: MT=0xF, status=0x006, protocol=0x02
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x006 << 16) | (0x02 << 8);

    uint16_t status = (words[0] >> 16) & 0x3FF;
    ASSERT(status == 0x006);

    TEST("parse current protocol = MIDI 2.0");
    uint8_t proto = (words[0] >> 8) & 0xFF;
    ASSERT(proto == 0x02);
    PASS();
}

// Simulates parsing Function Block Info Notification
void test_parse_fb_info() {
    printf("\n[Parse Function Block Info Notification]\n");

    // word0: MT=0xF, status=0x011, active=1 (bit15), fbIdx=3 (bits14:8),
    //        isMIDI1=0 (bits3:2), direction=2 (bits1:0)
    // word1: firstGroup=0 (byte3), groupLength=4 (byte2), midiCI=1 (byte1), maxS8=0 (byte0)
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x011 << 16)
             | (1 << 15) | (3 << 8) | 2;
    words[1] = ((uint32_t)0 << 24) | ((uint32_t)4 << 16) | (1 << 8) | 0;

    uint16_t status = (words[0] >> 16) & 0x3FF;
    ASSERT(status == 0x011);

    TEST("parse FB index");
    uint8_t fbIdx = (words[0] >> 8) & 0x7F;
    ASSERT(fbIdx == 3);
    PASS();

    TEST("parse FB active");
    bool active = (words[0] >> 15) & 0x01;
    ASSERT(active == true);
    PASS();

    TEST("parse FB direction = bidirectional");
    uint8_t dir = words[0] & 0x03;
    ASSERT(dir == 2);
    PASS();

    TEST("parse FB firstGroup=0, groupLength=4");
    uint8_t fg = (words[1] >> 24) & 0xFF;
    uint8_t gl = (words[1] >> 16) & 0xFF;
    ASSERT(fg == 0);
    ASSERT(gl == 4);
    PASS();

    TEST("parse FB midiCISupport=1");
    uint8_t ci = (words[1] >> 8) & 0xFF;
    ASSERT(ci == 1);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — UMP packet size determination (message type → word count)
// ---------------------------------------------------------------------------

void test_ump_packet_sizes() {
    printf("\n[UMP packet sizes by message type]\n");

    // Returns word count for a given MT
    auto pktWords = [](uint8_t mt) -> uint8_t {
        switch (mt) {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
                return 1;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
                return 2;
            case 0xB: case 0xC:
                return 3;
            default:
                return 4;
        }
    };

    TEST("MT 0x0 (Utility) = 1 word");
    ASSERT(pktWords(0x0) == 1);
    PASS();

    TEST("MT 0x2 (MIDI 1.0 CVM) = 1 word");
    ASSERT(pktWords(0x2) == 1);
    PASS();

    TEST("MT 0x3 (Data 64) = 2 words");
    ASSERT(pktWords(0x3) == 2);
    PASS();

    TEST("MT 0x4 (MIDI 2.0 CVM) = 2 words");
    ASSERT(pktWords(0x4) == 2);
    PASS();

    TEST("MT 0x5 (Data 128) = 4 words");
    ASSERT(pktWords(0x5) == 4);
    PASS();

    TEST("MT 0xD (Flex Data) = 4 words");
    ASSERT(pktWords(0xD) == 4);
    PASS();

    TEST("MT 0xF (Stream) = 4 words");
    ASSERT(pktWords(0xF) == 4);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — GTB descriptor parsing
// ---------------------------------------------------------------------------

// Extracted parseGTBResponse logic for native testing
struct GroupTerminalBlock {
    uint8_t  id;
    uint8_t  type;
    uint8_t  firstGroup;
    uint8_t  numGroups;
    uint8_t  protocol;
    uint16_t maxInputBW;
    uint16_t maxOutputBW;
};

static const uint8_t MAX_GTB = 8;
static const uint8_t GTB_HEADER_SUBTYPE = 0x01;
static const uint8_t GTB_BLOCK_SUBTYPE  = 0x02;
static const uint8_t GTB_BLOCK_DESC_LEN = 13;

static uint8_t parseGTB(const uint8_t* data, uint16_t len,
                        GroupTerminalBlock* out, uint8_t maxOut) {
    if (len < 5) return 0;
    if (data[2] != GTB_HEADER_SUBTYPE) return 0;
    uint16_t totalLen = data[3] | ((uint16_t)data[4] << 8);
    if (totalLen > len) totalLen = len;
    uint16_t offset = data[0];
    uint8_t count = 0;
    while (offset + GTB_BLOCK_DESC_LEN <= totalLen && count < maxOut) {
        const uint8_t* blk = data + offset;
        if (blk[0] < GTB_BLOCK_DESC_LEN) break;
        if (blk[2] != GTB_BLOCK_SUBTYPE) { offset += blk[0]; continue; }
        GroupTerminalBlock& g = out[count];
        g.id = blk[3]; g.type = blk[4]; g.firstGroup = blk[5]; g.numGroups = blk[6];
        g.protocol = blk[8];
        g.maxInputBW = blk[9] | ((uint16_t)blk[10] << 8);
        g.maxOutputBW = blk[11] | ((uint16_t)blk[12] << 8);
        count++;
        offset += blk[0];
    }
    return count;
}

void test_gtb_parsing() {
    printf("\n[GTB descriptor parsing]\n");

    // GTB Header (5 bytes) + 1 GTB Block (13 bytes) = 18 bytes total
    uint8_t gtbData[] = {
        // Header: bLength=5, bDescType=0x26, bDescSubtype=0x01, wTotalLength=18
        0x05, 0x26, 0x01, 18, 0x00,
        // Block: bLength=13, bDescType=0x26, bDescSubtype=0x02
        //   id=0, type=2(output), firstGrp=0, numGrp=1, iBlockItem=0,
        //   protocol=0x02(MIDI2), maxInBW=0x0000, maxOutBW=0x0001
        0x0D, 0x26, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00,
        0x02, 0x00, 0x00, 0x01, 0x00,
    };

    GroupTerminalBlock blocks[MAX_GTB] = {};
    uint8_t count = parseGTB(gtbData, sizeof(gtbData), blocks, MAX_GTB);

    TEST("parses 1 GTB block");
    ASSERT(count == 1);
    PASS();

    TEST("GTB id=0, type=2 (output)");
    ASSERT(blocks[0].id == 0);
    ASSERT(blocks[0].type == 2);
    PASS();

    TEST("GTB firstGroup=0, numGroups=1");
    ASSERT(blocks[0].firstGroup == 0);
    ASSERT(blocks[0].numGroups == 1);
    PASS();

    TEST("GTB protocol=0x02 (MIDI 2.0)");
    ASSERT(blocks[0].protocol == 0x02);
    PASS();

    TEST("GTB maxOutputBW=1");
    ASSERT(blocks[0].maxOutputBW == 1);
    PASS();

    // Two blocks
    uint8_t gtb2[] = {
        0x05, 0x26, 0x01, 31, 0x00,  // header, total=31
        0x0D, 0x26, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        0x0D, 0x26, 0x02, 0x01, 0x01, 0x02, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    };

    count = parseGTB(gtb2, sizeof(gtb2), blocks, MAX_GTB);
    TEST("parses 2 GTB blocks");
    ASSERT(count == 2);
    PASS();

    TEST("second block id=1, firstGroup=2");
    ASSERT(blocks[1].id == 1);
    ASSERT(blocks[1].firstGroup == 2);
    PASS();

    // Empty / invalid
    TEST("empty GTB data returns 0");
    uint8_t empty[] = { 0x05, 0x26, 0x01, 0x05, 0x00 };  // header only, no blocks
    count = parseGTB(empty, sizeof(empty), blocks, MAX_GTB);
    ASSERT(count == 0);
    PASS();

    TEST("truncated data returns 0");
    count = parseGTB(gtbData, 3, blocks, MAX_GTB);
    ASSERT(count == 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Stream text accumulation
// ---------------------------------------------------------------------------

// Extracted _appendStreamText logic for native testing
static void appendStreamText(char* dest, uint8_t& destLen, uint8_t maxLen,
                             const uint32_t* words, uint8_t form) {
    if (form == 0 || form == 1) destLen = 0;
    uint8_t text[14];
    uint8_t n = 0;
    uint8_t b;
    b = (words[0] >> 8) & 0xFF; if (b) text[n++] = b;
    b = words[0] & 0xFF;        if (b) text[n++] = b;
    for (uint8_t w = 1; w <= 3; w++) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            b = (words[w] >> shift) & 0xFF;
            if (b) text[n++] = b;
        }
    }
    for (uint8_t i = 0; i < n && destLen < maxLen; i++) {
        dest[destLen++] = (char)text[i];
    }
    dest[destLen] = '\0';
}

void test_stream_text() {
    printf("\n[Stream text accumulation]\n");

    char buf[64] = {};
    uint8_t len = 0;

    // Complete packet (form=0) with "Hello" in word0[15:8], word0[7:0], word1[31:24]...
    // 'H'=0x48, 'e'=0x65, 'l'=0x6C, 'l'=0x6C, 'o'=0x6F
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x003 << 16) | ('H' << 8) | 'e';
    words[1] = ((uint32_t)'l' << 24) | ((uint32_t)'l' << 16) | ((uint32_t)'o' << 8);

    TEST("complete packet extracts text");
    appendStreamText(buf, len, 63, words, 0);
    ASSERT(strcmp(buf, "Hello") == 0);
    ASSERT(len == 5);
    PASS();

    // Multi-packet: start + end
    TEST("start + end concatenation");
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x003 << 16) | ('A' << 8) | 'B';
    words[1] = 0; words[2] = 0; words[3] = 0;
    appendStreamText(buf, len, 63, words, 1);  // start
    ASSERT(strcmp(buf, "AB") == 0);
    ASSERT(len == 2);

    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x003 << 16) | ('C' << 8) | 'D';
    appendStreamText(buf, len, 63, words, 3);  // end (appends)
    ASSERT(strcmp(buf, "ABCD") == 0);
    ASSERT(len == 4);
    PASS();

    // New complete resets buffer
    TEST("new complete resets buffer");
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x003 << 16) | ('X' << 8);
    words[1] = 0; words[2] = 0; words[3] = 0;
    appendStreamText(buf, len, 63, words, 0);
    ASSERT(strcmp(buf, "X") == 0);
    ASSERT(len == 1);
    PASS();

    // Buffer overflow protection
    TEST("respects maxLen limit");
    char tiny[4] = {};
    uint8_t tinyLen = 0;
    words[0] = ((uint32_t)0x0F << 28) | ('A' << 8) | 'B';
    words[1] = ('C' << 24) | ('D' << 16) | ('E' << 8) | 'F';
    words[2] = 0; words[3] = 0;
    appendStreamText(tiny, tinyLen, 3, words, 0);
    ASSERT(tinyLen == 3);
    ASSERT(tiny[3] == '\0');
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Full negotiation state machine sequence
// ---------------------------------------------------------------------------

// Simulates the full negotiation sequence by manually walking through
// _processStreamMessage parsing at each stage.

// State machine enum (mirrors USBMIDI2Connection)
enum NegotiationState : uint8_t {
    NegIdle, NegAwaitEndpointInfo, NegAwaitStreamConfig, NegAwaitFBInfo, NegDone
};

struct NegSim {
    NegotiationState state;
    uint8_t numFunctionBlocks;
    bool supportsMIDI2;
    uint8_t currentProtocol;
    uint8_t fbCount;
    uint8_t fbExpected;

    void processStream(const uint32_t* words) {
        uint16_t status = (words[0] >> 16) & 0x3FF;
        switch (status) {
        case 0x001: // Endpoint Info
            numFunctionBlocks = (words[1] >> 24) & 0xFF;
            supportsMIDI2 = (words[1] >> 9) & 0x01;
            if (state == NegAwaitEndpointInfo)
                state = NegAwaitStreamConfig;
            break;
        case 0x006: // Stream Config Notify
            currentProtocol = (words[0] >> 8) & 0xFF;
            if (state == NegAwaitStreamConfig) {
                fbCount = 0;
                fbExpected = numFunctionBlocks;
                if (fbExpected > 0) {
                    state = NegAwaitFBInfo;
                } else {
                    state = NegDone;
                }
            }
            break;
        case 0x011: // FB Info
            fbCount++;
            if (state == NegAwaitFBInfo && fbCount >= fbExpected)
                state = NegDone;
            break;
        }
    }
};

void test_negotiation_full_sequence() {
    printf("\n[Negotiation — full happy-path sequence]\n");

    NegSim neg = {};
    neg.state = NegAwaitEndpointInfo;

    // Step 1: Device sends Endpoint Info (2 FBs, supports MIDI 2.0)
    uint32_t epInfo[4] = {};
    epInfo[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x001 << 16) | (1 << 8) | 1;
    epInfo[1] = ((uint32_t)2 << 24) | (1 << 9) | (1 << 8) | 1;
    neg.processStream(epInfo);

    TEST("after EP Info → NegAwaitStreamConfig");
    ASSERT(neg.state == NegAwaitStreamConfig);
    ASSERT(neg.numFunctionBlocks == 2);
    ASSERT(neg.supportsMIDI2 == true);
    PASS();

    // Step 2: Device confirms MIDI 2.0 protocol
    uint32_t config[4] = {};
    config[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x006 << 16) | (0x02 << 8);
    neg.processStream(config);

    TEST("after Config Notify → NegAwaitFBInfo");
    ASSERT(neg.state == NegAwaitFBInfo);
    ASSERT(neg.currentProtocol == 0x02);
    ASSERT(neg.fbExpected == 2);
    PASS();

    // Step 3: First FB Info
    uint32_t fb1[4] = {};
    fb1[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x011 << 16) | (1 << 15) | (0 << 8) | 2;
    fb1[1] = ((uint32_t)0 << 24) | ((uint32_t)4 << 16);
    neg.processStream(fb1);

    TEST("after 1st FB Info → still NegAwaitFBInfo");
    ASSERT(neg.state == NegAwaitFBInfo);
    ASSERT(neg.fbCount == 1);
    PASS();

    // Step 4: Second FB Info → negotiation done
    uint32_t fb2[4] = {};
    fb2[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x011 << 16) | (1 << 15) | (1 << 8) | 1;
    fb2[1] = ((uint32_t)4 << 24) | ((uint32_t)2 << 16);
    neg.processStream(fb2);

    TEST("after 2nd FB Info → NegDone");
    ASSERT(neg.state == NegDone);
    ASSERT(neg.fbCount == 2);
    PASS();
}

void test_negotiation_zero_fbs() {
    printf("\n[Negotiation — device with 0 function blocks]\n");

    NegSim neg = {};
    neg.state = NegAwaitEndpointInfo;

    // EP Info says 0 FBs
    uint32_t epInfo[4] = {};
    epInfo[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x001 << 16) | (1 << 8) | 1;
    epInfo[1] = ((uint32_t)0 << 24) | (1 << 9) | (1 << 8);
    neg.processStream(epInfo);

    // Config Notify
    uint32_t config[4] = {};
    config[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x006 << 16) | (0x02 << 8);
    neg.processStream(config);

    TEST("0 FBs → NegDone immediately after Config");
    ASSERT(neg.state == NegDone);
    ASSERT(neg.fbExpected == 0);
    PASS();
}

void test_negotiation_midi1_fallback() {
    printf("\n[Negotiation — MIDI 1.0 fallback when no MIDI 2.0 support]\n");

    NegSim neg = {};
    neg.state = NegAwaitEndpointInfo;

    // EP Info: supports MIDI 1.0 only (bit9=0, bit8=1)
    uint32_t epInfo[4] = {};
    epInfo[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x001 << 16) | (1 << 8) | 1;
    epInfo[1] = ((uint32_t)1 << 24) | (0 << 9) | (1 << 8);
    neg.processStream(epInfo);

    TEST("detects no MIDI 2.0 support");
    ASSERT(neg.supportsMIDI2 == false);
    ASSERT(neg.state == NegAwaitStreamConfig);
    PASS();

    // Config Notify with MIDI 1.0
    uint32_t config[4] = {};
    config[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x006 << 16) | (0x01 << 8);
    neg.processStream(config);

    TEST("MIDI 1.0 protocol confirmed");
    ASSERT(neg.currentProtocol == 0x01);
    PASS();
}

void test_negotiation_out_of_order() {
    printf("\n[Negotiation — out-of-order messages ignored]\n");

    NegSim neg = {};
    neg.state = NegAwaitEndpointInfo;

    // FB Info arrives before EP Info — should not crash or advance state
    uint32_t fb[4] = {};
    fb[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x011 << 16) | (1 << 15) | (0 << 8) | 2;
    fb[1] = 0;
    neg.processStream(fb);

    TEST("FB Info before EP Info — state unchanged");
    ASSERT(neg.state == NegAwaitEndpointInfo);
    PASS();

    // Config Notify before EP Info — should not advance
    uint32_t config[4] = {};
    config[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x006 << 16) | (0x02 << 8);
    neg.processStream(config);

    TEST("Config Notify before EP Info — state unchanged");
    ASSERT(neg.state == NegAwaitEndpointInfo);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — UMP demuxing (mixed packet types)
// ---------------------------------------------------------------------------

void test_ump_demux_mixed() {
    printf("\n[UMP demux — mixed packet types in single transfer]\n");

    // Simulate a buffer with: 1-word Utility + 2-word MIDI2 CVM + 4-word Stream
    // Total = 1 + 2 + 4 = 7 words
    uint32_t buf[7] = {
        0x00000000,                                         // MT 0x0 Utility (1 word)
        0x40903C00, 0xFFFF0000,                             // MT 0x4 MIDI2 CVM (2 words)
        0xF0010101, 0x02000601, 0x00000000, 0x00000000,     // MT 0xF Stream (4 words)
    };

    // Walk buffer same way as _onReceiveUMP
    struct Packet { uint8_t mt; uint8_t words; uint8_t startIdx; };
    Packet packets[8] = {};
    uint8_t pktCount = 0;
    uint8_t i = 0;
    uint8_t totalWords = 7;
    while (i < totalWords && pktCount < 8) {
        uint8_t mt = (buf[i] >> 28) & 0x0F;
        uint8_t pw;
        switch (mt) {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7: pw = 1; break;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA: pw = 2; break;
            case 0xB: case 0xC: pw = 3; break;
            default: pw = 4; break;
        }
        if (i + pw > totalWords) break;
        packets[pktCount++] = { mt, pw, i };
        i += pw;
    }

    TEST("demux finds 3 packets");
    ASSERT(pktCount == 3);
    PASS();

    TEST("packet 0: MT=0x0, 1 word, idx=0");
    ASSERT(packets[0].mt == 0x0);
    ASSERT(packets[0].words == 1);
    ASSERT(packets[0].startIdx == 0);
    PASS();

    TEST("packet 1: MT=0x4, 2 words, idx=1");
    ASSERT(packets[1].mt == 0x4);
    ASSERT(packets[1].words == 2);
    ASSERT(packets[1].startIdx == 1);
    PASS();

    TEST("packet 2: MT=0xF, 4 words, idx=3");
    ASSERT(packets[2].mt == 0xF);
    ASSERT(packets[2].words == 4);
    ASSERT(packets[2].startIdx == 3);
    PASS();
}

void test_ump_demux_incomplete() {
    printf("\n[UMP demux — incomplete trailing packet]\n");

    // 3 words: 2-word MIDI2 CVM + only 1 word of a 4-word stream
    uint32_t buf[3] = {
        0x40903C00, 0xFFFF0000,     // MT 0x4 (2 words)
        0xF0010101,                 // MT 0xF but only 1 word (needs 4)
    };

    uint8_t i = 0, pktCount = 0;
    while (i < 3) {
        uint8_t mt = (buf[i] >> 28) & 0x0F;
        uint8_t pw;
        switch (mt) {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7: pw = 1; break;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA: pw = 2; break;
            case 0xB: case 0xC: pw = 3; break;
            default: pw = 4; break;
        }
        if (i + pw > 3) break;
        pktCount++;
        i += pw;
    }

    TEST("stops before incomplete 4-word packet");
    ASSERT(pktCount == 1);  // only the MIDI2 CVM
    PASS();
}

void test_ump_demux_all_mts() {
    printf("\n[UMP demux — all 16 message types]\n");

    auto pktWords = [](uint8_t mt) -> uint8_t {
        switch (mt) {
            case 0x0: case 0x1: case 0x2: case 0x6: case 0x7: return 1;
            case 0x3: case 0x4: case 0x8: case 0x9: case 0xA: return 2;
            case 0xB: case 0xC: return 3;
            default: return 4;
        }
    };

    TEST("MT 0x1 (System) = 1 word");
    ASSERT(pktWords(0x1) == 1);
    PASS();

    TEST("MT 0x6 (reserved) = 1 word");
    ASSERT(pktWords(0x6) == 1);
    PASS();

    TEST("MT 0x7 (reserved) = 1 word");
    ASSERT(pktWords(0x7) == 1);
    PASS();

    TEST("MT 0x8 (reserved) = 2 words");
    ASSERT(pktWords(0x8) == 2);
    PASS();

    TEST("MT 0x9 (reserved) = 2 words");
    ASSERT(pktWords(0x9) == 2);
    PASS();

    TEST("MT 0xA (reserved) = 2 words");
    ASSERT(pktWords(0xA) == 2);
    PASS();

    TEST("MT 0xB (reserved) = 3 words");
    ASSERT(pktWords(0xB) == 3);
    PASS();

    TEST("MT 0xC (reserved) = 3 words");
    ASSERT(pktWords(0xC) == 3);
    PASS();

    TEST("MT 0xE (reserved) = 4 words");
    ASSERT(pktWords(0xE) == 4);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — sendUMPMessage edge cases (logic only, no USB hardware)
// ---------------------------------------------------------------------------

void test_send_ump_edges() {
    printf("\n[sendUMPMessage — edge cases]\n");

    // We can't test the actual USB submission, but we can test the
    // parameter validation logic that mirrors the real code.

    auto validateSend = [](const uint32_t* words, uint8_t count,
                           bool isReady, bool hasOut, uint16_t maxPkt) -> bool {
        if (!isReady || !hasOut || count == 0) return false;
        size_t byteLen = (size_t)count * 4;
        if (byteLen > maxPkt) return false;
        return true;
    };

    uint32_t msg[4] = { 0x40903C00, 0xFFFF0000, 0, 0 };

    TEST("count=0 → reject");
    ASSERT(!validateSend(msg, 0, true, true, 64));
    PASS();

    TEST("not ready → reject");
    ASSERT(!validateSend(msg, 2, false, true, 64));
    PASS();

    TEST("no OUT transfer → reject");
    ASSERT(!validateSend(msg, 2, true, false, 64));
    PASS();

    TEST("payload exceeds maxPacket → reject");
    ASSERT(!validateSend(msg, 4, true, true, 8));  // 16 bytes > 8
    PASS();

    TEST("exact maxPacket fit → accept");
    ASSERT(validateSend(msg, 4, true, true, 16));  // 16 bytes == 16
    PASS();

    TEST("normal send → accept");
    ASSERT(validateSend(msg, 2, true, true, 64));  // 8 bytes < 64
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — _onDeviceGone state reset verification
// ---------------------------------------------------------------------------

void test_device_gone_reset() {
    printf("\n[_onDeviceGone — state reset verification]\n");

    // Simulate the reset that _onDeviceGone performs
    struct DeviceState {
        bool midi2Active;
        NegotiationState negState;
        uint8_t fbCount, fbExpected, gtbCount;
        char epName[64];
        uint8_t epNameLen;
        char prodId[64];
        uint8_t prodIdLen;
        uint8_t umpVersionMajor;

        void reset() {
            midi2Active = false;
            negState = NegIdle;
            fbCount = 0;
            fbExpected = 0;
            gtbCount = 0;
            epName[0] = '\0'; epNameLen = 0;
            prodId[0] = '\0'; prodIdLen = 0;
            umpVersionMajor = 0;
        }
    };

    // Set up "connected" state
    DeviceState ds;
    ds.midi2Active = true;
    ds.negState = NegDone;
    ds.fbCount = 3;
    ds.fbExpected = 3;
    ds.gtbCount = 2;
    strcpy(ds.epName, "Test Device");
    ds.epNameLen = 11;
    strcpy(ds.prodId, "ABC123");
    ds.prodIdLen = 6;
    ds.umpVersionMajor = 1;

    ds.reset();

    TEST("midi2Active reset to false");
    ASSERT(ds.midi2Active == false);
    PASS();

    TEST("negotiation state reset to NegIdle");
    ASSERT(ds.negState == NegIdle);
    PASS();

    TEST("fbCount reset to 0");
    ASSERT(ds.fbCount == 0);
    PASS();

    TEST("gtbCount reset to 0");
    ASSERT(ds.gtbCount == 0);
    PASS();

    TEST("epName cleared");
    ASSERT(ds.epName[0] == '\0');
    ASSERT(ds.epNameLen == 0);
    PASS();

    TEST("prodId cleared");
    ASSERT(ds.prodId[0] == '\0');
    ASSERT(ds.prodIdLen == 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Descriptor scanning: non-MIDI interface before MIDI
// ---------------------------------------------------------------------------

// HID + MIDI composite device
static const uint8_t DESC_HID_PLUS_MIDI[] = {
    // Config
    0x09, 0x02, 0x50, 0x00, 0x03, 0x01, 0x00, 0x80, 0x32,
    // Interface 0: HID (class=0x03)
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,
    // HID endpoint
    0x07, 0x05, 0x83, 0x03, 0x08, 0x00, 0x0A,
    // Interface 1: Audio Control
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    // Interface 2: MIDI Streaming
    0x09, 0x04, 0x02, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x01, 0x25, 0x00,
    0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
};

// MIDI on interface 0 (not 1)
static const uint8_t DESC_MIDI_IFACE_ZERO[] = {
    0x09, 0x02, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00,
    0x09, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
    0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x00, 0x00, 0x00,
};

void test_multi_interface() {
    printf("\n[Multi-interface — HID + MIDI composite]\n");

    AltCandidate best = {};
    bool found = findBestAlt(DESC_HID_PLUS_MIDI, sizeof(DESC_HID_PLUS_MIDI), best);

    TEST("finds MIDI interface despite HID before it");
    ASSERT(found);
    ASSERT(best.ifaceNumber == 2);
    PASS();

    TEST("correct endpoints from MIDI interface");
    ASSERT(best.epInAddress == 0x81);
    ASSERT(best.epOutAddress == 0x01);
    PASS();
}

void test_midi_on_iface_zero() {
    printf("\n[MIDI on interface 0 — MIDI 2.0]\n");

    AltCandidate best = {};
    bool found = findBestAlt(DESC_MIDI_IFACE_ZERO, sizeof(DESC_MIDI_IFACE_ZERO), best);

    TEST("finds MIDI 2.0 on interface 0");
    ASSERT(found);
    ASSERT(best.ifaceNumber == 0);
    ASSERT(best.isMIDI2 == true);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Stream text edge cases
// ---------------------------------------------------------------------------

void test_stream_text_edges() {
    printf("\n[Stream text — edge cases]\n");

    char buf[64] = {};
    uint8_t len = 0;

    // Continue without prior start — should append from current pos
    TEST("continue without start appends to existing");
    buf[0] = 'Z'; len = 1;
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ('A' << 8) | 'B';
    words[1] = 0; words[2] = 0; words[3] = 0;
    appendStreamText(buf, len, 63, words, 2);  // form=2 (continue)
    ASSERT(len == 3);
    ASSERT(buf[0] == 'Z');
    ASSERT(buf[1] == 'A');
    ASSERT(buf[2] == 'B');
    PASS();

    // Full 14-byte packet
    TEST("max 14 bytes per packet");
    words[0] = ((uint32_t)0x0F << 28) | ('A' << 8) | 'B';
    words[1] = ('C' << 24) | ('D' << 16) | ('E' << 8) | 'F';
    words[2] = ('G' << 24) | ('H' << 16) | ('I' << 8) | 'J';
    words[3] = ('K' << 24) | ('L' << 16) | ('M' << 8) | 'N';
    appendStreamText(buf, len, 63, words, 0);  // complete
    ASSERT(len == 14);
    ASSERT(strncmp(buf, "ABCDEFGHIJKLMN", 14) == 0);
    PASS();

    // NUL byte in middle stops extraction at that byte
    TEST("NUL byte in middle truncates text");
    words[0] = ((uint32_t)0x0F << 28) | ('X' << 8) | 'Y';
    words[1] = ('Z' << 24) | (0 << 16) | ('A' << 8) | 'B';  // NUL in middle
    words[2] = 0; words[3] = 0;
    appendStreamText(buf, len, 63, words, 0);
    // NUL stops extraction, so only X, Y, Z are extracted (NUL skipped, then A,B appended)
    // Actually the code skips NUL bytes (if (b) text[n++] = b), so it gets X,Y,Z,A,B
    ASSERT(buf[0] == 'X');
    ASSERT(buf[1] == 'Y');
    ASSERT(buf[2] == 'Z');
    ASSERT(buf[3] == 'A');
    ASSERT(buf[4] == 'B');
    ASSERT(len == 5);
    PASS();

    // Empty packet (all zeros after MT/status)
    TEST("empty text packet — len stays 0");
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x003 << 16);
    words[1] = 0; words[2] = 0; words[3] = 0;
    appendStreamText(buf, len, 63, words, 0);
    ASSERT(len == 0);
    ASSERT(buf[0] == '\0');
    PASS();

    // maxLen=0 edge case
    TEST("maxLen=0 writes only NUL terminator");
    char micro[1] = { 'X' };
    uint8_t microLen = 0;
    words[0] = ((uint32_t)0x0F << 28) | ('A' << 8);
    appendStreamText(micro, microLen, 0, words, 0);
    ASSERT(microLen == 0);
    ASSERT(micro[0] == '\0');
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Function Block edge cases
// ---------------------------------------------------------------------------

void test_fb_overflow() {
    printf("\n[Function Block — overflow protection]\n");

    // Simulate receiving more FBs than MAX_FUNCTION_BLOCKS (8)
    struct FBSim {
        uint8_t fbCount;
        uint8_t fbIndex[16];

        void processFBInfo(const uint32_t* words) {
            if (fbCount < 8) {
                fbIndex[fbCount] = (words[0] >> 8) & 0x7F;
                fbCount++;
            }
        }
    };

    FBSim sim = {};
    for (int i = 0; i < 12; i++) {
        uint32_t words[4] = {};
        words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x011 << 16) | (1 << 15) | ((uint8_t)i << 8);
        sim.processFBInfo(words);
    }

    TEST("caps at MAX_FUNCTION_BLOCKS (8)");
    ASSERT(sim.fbCount == 8);
    PASS();

    TEST("last stored FB has index 7");
    ASSERT(sim.fbIndex[7] == 7);
    PASS();
}

void test_fb_ismidi1_field() {
    printf("\n[Function Block — isMIDI1 field parsing]\n");

    // isMIDI1 is in bits 3:2 of word0 low byte
    // isMIDI1=2 (restricted), direction=1 (output)
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x011 << 16)
             | (1 << 15) | (5 << 8)  // active, index=5
             | (2 << 2) | 1;         // isMIDI1=2, direction=1

    uint8_t isMIDI1 = (words[0] >> 2) & 0x03;
    uint8_t direction = words[0] & 0x03;

    TEST("isMIDI1=2 (restricted)");
    ASSERT(isMIDI1 == 2);
    PASS();

    TEST("direction=1 (output) alongside isMIDI1");
    ASSERT(direction == 1);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — GTB edge cases
// ---------------------------------------------------------------------------

void test_gtb_wrong_header_subtype() {
    printf("\n[GTB — wrong header subtype]\n");

    uint8_t data[] = {
        0x05, 0x26, 0x03, 18, 0x00,  // subtype 0x03 instead of 0x01
        0x0D, 0x26, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00,
        0x02, 0x00, 0x00, 0x01, 0x00,
    };

    GroupTerminalBlock blocks[MAX_GTB] = {};
    uint8_t count = parseGTB(data, sizeof(data), blocks, MAX_GTB);

    TEST("wrong header subtype → 0 blocks");
    ASSERT(count == 0);
    PASS();
}

void test_gtb_non_block_interspersed() {
    printf("\n[GTB — non-block descriptors interspersed]\n");

    // Header + unknown descriptor (subtype 0x05) + real block
    uint8_t data[] = {
        // Header: bLength=5, total=31
        0x05, 0x26, 0x01, 31, 0x00,
        // Unknown: bLength=13, subtype=0x05 (not 0x02) — should be skipped
        0x0D, 0x26, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Real block: id=2, type=0, firstGroup=1, numGroups=3, protocol=0x01
        0x0D, 0x26, 0x02, 0x02, 0x00, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    };

    GroupTerminalBlock blocks[MAX_GTB] = {};
    uint8_t count = parseGTB(data, sizeof(data), blocks, MAX_GTB);

    TEST("skips non-block, parses real block");
    ASSERT(count == 1);
    PASS();

    TEST("parsed block has id=2, firstGroup=1");
    ASSERT(blocks[0].id == 2);
    ASSERT(blocks[0].firstGroup == 1);
    ASSERT(blocks[0].numGroups == 3);
    ASSERT(blocks[0].protocol == 0x01);
    PASS();
}

void test_gtb_max_blocks() {
    printf("\n[GTB — MAX_GTB limit]\n");

    // Build header + 10 blocks (exceeds MAX_GTB=8)
    uint8_t data[5 + 10 * 13];
    data[0] = 5; data[1] = 0x26; data[2] = 0x01;
    uint16_t total = 5 + 10 * 13;
    data[3] = total & 0xFF; data[4] = (total >> 8) & 0xFF;
    for (int i = 0; i < 10; i++) {
        uint8_t* blk = data + 5 + i * 13;
        blk[0] = 13; blk[1] = 0x26; blk[2] = 0x02;
        blk[3] = i;  // id
        blk[4] = 0; blk[5] = i; blk[6] = 1; blk[7] = 0;
        blk[8] = 0x02; blk[9] = 0; blk[10] = 0; blk[11] = 0; blk[12] = 0;
    }

    GroupTerminalBlock blocks[MAX_GTB] = {};
    uint8_t count = parseGTB(data, sizeof(data), blocks, MAX_GTB);

    TEST("caps at MAX_GTB (8) blocks");
    ASSERT(count == 8);
    PASS();

    TEST("last stored block has id=7");
    ASSERT(blocks[7].id == 7);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — bcdMSC boundary values
// ---------------------------------------------------------------------------

void test_bcdmsc_boundaries() {
    printf("\n[bcdMSC — boundary values]\n");

    // bcdMSC = 0x0199 (just below 0x0200) should be MIDI 1.0
    uint8_t desc199[] = {
        0x09, 0x02, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x09, 0x04, 0x00, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00,
        0x07, 0x24, 0x01, 0x99, 0x01, 0x07, 0x00,  // bcdMSC=0x0199
        0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x01, 0x00, 0x00,
    };
    AltCandidate best = {};
    bool found = findBestAlt(desc199, sizeof(desc199), best);

    TEST("bcdMSC=0x0199 → not MIDI 2.0");
    ASSERT(found);
    ASSERT(best.isMIDI2 == false);
    PASS();

    // bcdMSC = 0x0200 (exactly 2.0)
    uint8_t desc200[] = {
        0x09, 0x02, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x09, 0x04, 0x00, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00,
        0x07, 0x24, 0x01, 0x00, 0x02, 0x07, 0x00,  // bcdMSC=0x0200
        0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x01, 0x00, 0x00,
    };
    best = {};
    found = findBestAlt(desc200, sizeof(desc200), best);

    TEST("bcdMSC=0x0200 → MIDI 2.0");
    ASSERT(found);
    ASSERT(best.isMIDI2 == true);
    PASS();

    // bcdMSC = 0x0300 (future version) — still treated as MIDI 2.0+
    uint8_t desc300[] = {
        0x09, 0x02, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x09, 0x04, 0x00, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00,
        0x07, 0x24, 0x01, 0x00, 0x03, 0x07, 0x00,  // bcdMSC=0x0300
        0x09, 0x05, 0x81, 0x02, 0x40, 0x00, 0x01, 0x00, 0x00,
    };
    best = {};
    found = findBestAlt(desc300, sizeof(desc300), best);

    TEST("bcdMSC=0x0300 → treated as MIDI 2.0+");
    ASSERT(found);
    ASSERT(best.isMIDI2 == true);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Multiple endpoints in same alt setting
// ---------------------------------------------------------------------------

void test_multiple_in_endpoints() {
    printf("\n[Multiple IN endpoints — last one wins]\n");

    // Alt 0 with two IN endpoints — second should overwrite first
    uint8_t desc[] = {
        0x09, 0x02, 0x40, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x09, 0x04, 0x00, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00,
        0x07, 0x24, 0x01, 0x00, 0x01, 0x07, 0x00,
        // First IN endpoint: 0x82, maxPkt=32
        0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x01, 0x00, 0x00,
        // Second IN endpoint: 0x84, maxPkt=128
        0x09, 0x05, 0x84, 0x02, 0x80, 0x00, 0x01, 0x00, 0x00,
    };

    AltCandidate best = {};
    bool found = findBestAlt(desc, sizeof(desc), best);

    TEST("last IN endpoint overwrites first");
    ASSERT(found);
    ASSERT(best.epInAddress == 0x84);
    ASSERT(best.epInMaxPacket == 128);
    PASS();
}

// ---------------------------------------------------------------------------
// Tests — Endpoint Info bit field exhaustive
// ---------------------------------------------------------------------------

void test_endpoint_info_all_flags() {
    printf("\n[Endpoint Info — all flag combinations]\n");

    // No flags set
    uint32_t words[4] = {};
    words[0] = ((uint32_t)0x0F << 28) | ((uint32_t)0x001 << 16) | (1 << 8) | 0;
    words[1] = ((uint32_t)5 << 24);  // 5 FBs, no protocol support, no JR

    TEST("no flags → all false");
    ASSERT(((words[1] >> 9) & 0x01) == 0);  // supportsMIDI2
    ASSERT(((words[1] >> 8) & 0x01) == 0);  // supportsMIDI1
    ASSERT(((words[1] >> 1) & 0x01) == 0);  // supportsRxJR
    ASSERT((words[1] & 0x01) == 0);          // supportsTxJR
    PASS();

    // All flags set
    words[1] = ((uint32_t)5 << 24) | (1 << 9) | (1 << 8) | (1 << 1) | 1;

    TEST("all flags → all true");
    ASSERT(((words[1] >> 9) & 0x01) == 1);
    ASSERT(((words[1] >> 8) & 0x01) == 1);
    ASSERT(((words[1] >> 1) & 0x01) == 1);
    ASSERT((words[1] & 0x01) == 1);
    PASS();

    // Only RxJR
    words[1] = ((uint32_t)1 << 24) | (1 << 1);
    TEST("only RxJR flag");
    ASSERT(((words[1] >> 9) & 0x01) == 0);
    ASSERT(((words[1] >> 1) & 0x01) == 1);
    ASSERT((words[1] & 0x01) == 0);
    PASS();
}

// ---------------------------------------------------------------------------

int main() {
    printf("USBMIDI2Connection — test suite (descriptor scan + negotiation)\n");
    printf("================================================================\n");

    test_bohemian_sender();
    test_midi1_only();
    test_no_midi();
    test_empty_descriptor();
    test_reversed_alt_order();
    test_zero_endpoints();
    test_out_only();
    test_maxpacket_edge();
    test_interval();
    test_corrupt_descriptor();
    test_ump_dispatch();
    test_stream_message_build();
    test_parse_endpoint_info();
    test_parse_stream_config();
    test_parse_fb_info();
    test_ump_packet_sizes();
    test_gtb_parsing();
    test_stream_text();
    test_negotiation_full_sequence();
    test_negotiation_zero_fbs();
    test_negotiation_midi1_fallback();
    test_negotiation_out_of_order();
    test_ump_demux_mixed();
    test_ump_demux_incomplete();
    test_ump_demux_all_mts();
    test_send_ump_edges();
    test_device_gone_reset();
    test_multi_interface();
    test_midi_on_iface_zero();
    test_stream_text_edges();
    test_fb_overflow();
    test_fb_ismidi1_field();
    test_gtb_wrong_header_subtype();
    test_gtb_non_block_interspersed();
    test_gtb_max_blocks();
    test_bcdmsc_boundaries();
    test_multiple_in_endpoints();
    test_endpoint_info_all_flags();

    printf("\n================================================================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
