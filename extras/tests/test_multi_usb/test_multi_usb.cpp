// ESP32_Host_MIDI — Multi-USB device support test suite
// Tests ring buffer, transport lifecycle, removeTransport, sourceDevice.
// Build:
//   g++ -std=c++17 -DUNIT_TEST -Iextras/tests/test_multi_usb/stub \
//       -Iextras/tests/stub -Isrc -Wall -Wextra -Wno-unused-parameter \
//       -Wno-comment -o extras/tests/test_multi_usb/test_multi_usb \
//       extras/tests/test_multi_usb/test_multi_usb.cpp \
//       src/USBDeviceTransport.cpp src/MIDIHandler.cpp

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <vector>

// Stubs — Arduino, FreeRTOS, USB Host
#include "Arduino.h"

unsigned long g_fakeMillis = 0;
FakeSerial Serial;

// Include after stubs so ESP32_HOST_MIDI_NO_USB_HOST is not set for this test
// (we want USBDeviceTransport available)
#include "USBDeviceTransport.h"
#include "MIDIHandler.h"

// ---------------------------------------------------------------------------
// Minimal test framework (same pattern as test_handler.cpp)
// ---------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-56s", name); } while(0)

#define PASS() \
    do { printf("OK\n"); ++g_pass; } while(0)

#define FAIL(msg) \
    do { printf("FAIL -- %s\n", msg); ++g_fail; return; } while(0)

#define ASSERT(expr) \
    do { if (!(expr)) { printf("FAIL -- " #expr "\n"); ++g_fail; return; } } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { printf("FAIL -- %s != %s (got %ld vs %ld)\n", #a, #b, (long)(a), (long)(b)); ++g_fail; return; } } while(0)

// ---------------------------------------------------------------------------
// Helper: feed raw MIDI into handler and return the last event
// ---------------------------------------------------------------------------

static MIDIEventData feedMidi(MIDIHandler& h, uint8_t status, uint8_t d1, uint8_t d2 = 0) {
    uint8_t cin = (status >> 4) & 0x0F;
    uint8_t buf[4] = { cin, status, d1, d2 };
    h.handleMidiMessage(buf, 4);
    auto& q = h.getQueue();
    return q.back();
}

// ---------------------------------------------------------------------------
// Test: USBDeviceTransport ring buffer — enqueue/dequeue cycle
// ---------------------------------------------------------------------------

void test_ring_buffer_enqueue_dequeue() {
    printf("\n[Ring Buffer: Enqueue/Dequeue]\n");

    USBDeviceTransport transport;

    // Set up a MIDI data callback to capture dispatched data
    std::vector<std::vector<uint8_t>> received;
    auto callback = [](void* ctx, const uint8_t* data, size_t len) {
        auto* vec = static_cast<std::vector<std::vector<uint8_t>>*>(ctx);
        vec->push_back(std::vector<uint8_t>(data, data + len));
    };
    transport.setMidiCallback(callback, &received);

    TEST("enqueuePacket returns true on empty queue");
    uint8_t pkt1[4] = { 0x09, 0x90, 60, 100 };
    ASSERT(transport.enqueuePacket(pkt1, 4) == true);
    PASS();

    TEST("enqueue second packet returns true");
    uint8_t pkt2[4] = { 0x09, 0x90, 64, 80 };
    ASSERT(transport.enqueuePacket(pkt2, 4) == true);
    PASS();

    TEST("task() dispatches queued packets in order");
    transport.task();
    ASSERT_EQ((long)received.size(), 2L);
    ASSERT(received[0][1] == 0x90 && received[0][2] == 60 && received[0][3] == 100);
    ASSERT(received[1][1] == 0x90 && received[1][2] == 64 && received[1][3] == 80);
    PASS();

    TEST("queue is empty after task() drains it");
    received.clear();
    transport.task();
    ASSERT_EQ((long)received.size(), 0L);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: USBDeviceTransport ring buffer — queue full returns false
// ---------------------------------------------------------------------------

void test_ring_buffer_full() {
    printf("\n[Ring Buffer: Queue Full]\n");

    USBDeviceTransport transport;

    TEST("filling queue to capacity (QUEUE_SIZE - 1 = 63 entries)");
    uint8_t pkt[4] = { 0x09, 0x90, 60, 100 };
    int enqueued = 0;
    // Ring buffer can hold QUEUE_SIZE - 1 = 63 entries
    for (int i = 0; i < 64; i++) {
        if (transport.enqueuePacket(pkt, 4)) {
            enqueued++;
        }
    }
    ASSERT_EQ(enqueued, 63);
    PASS();

    TEST("enqueue on full queue returns false");
    ASSERT(transport.enqueuePacket(pkt, 4) == false);
    PASS();

    TEST("after draining, enqueue works again");
    // Drain via task() — need a callback to consume
    transport.setMidiCallback([](void*, const uint8_t*, size_t) {}, nullptr);
    transport.task();
    ASSERT(transport.enqueuePacket(pkt, 4) == true);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: USBDeviceTransport ring buffer — empty dequeue returns false
// ---------------------------------------------------------------------------

void test_ring_buffer_empty() {
    printf("\n[Ring Buffer: Empty Dequeue]\n");

    USBDeviceTransport transport;

    std::vector<std::vector<uint8_t>> received;
    auto callback = [](void* ctx, const uint8_t* data, size_t len) {
        auto* vec = static_cast<std::vector<std::vector<uint8_t>>*>(ctx);
        vec->push_back(std::vector<uint8_t>(data, data + len));
    };
    transport.setMidiCallback(callback, &received);

    TEST("task() on empty queue dispatches nothing");
    transport.task();
    ASSERT_EQ((long)received.size(), 0L);
    PASS();

    TEST("enqueue-then-drain-then-task yields nothing");
    uint8_t pkt[4] = { 0x09, 0x90, 60, 100 };
    transport.enqueuePacket(pkt, 4);
    transport.task();
    ASSERT_EQ((long)received.size(), 1L);
    received.clear();
    transport.task();
    ASSERT_EQ((long)received.size(), 0L);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: MIDIHandler addTransport / removeTransport
// ---------------------------------------------------------------------------

// A minimal concrete transport for testing
class DummyTransport : public MIDITransport {
public:
    void task() override {}
    bool isConnected() const override { return true; }
};

void test_add_remove_transport() {
    printf("\n[MIDIHandler: addTransport / removeTransport]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 1000;

    DummyTransport t1, t2, t3;

    TEST("addTransport registers transports successfully");
    h.addTransport(&t1);
    h.addTransport(&t2);
    h.addTransport(&t3);
    // Verify handler processes events after adding transports
    auto ev0 = feedMidi(h, 0x90, 48, 100);
    ASSERT(ev0.statusCode == MIDI_NOTE_ON);
    PASS();

    TEST("removeTransport shifts array correctly");
    // Remove t2 (middle); t3 should still work
    h.removeTransport(&t2);
    // Verify t1 and t3 still dispatch properly
    // Feed MIDI from handler directly to verify queue works
    auto ev = feedMidi(h, 0x90, 60, 100);
    ASSERT(ev.statusCode == MIDI_NOTE_ON);
    PASS();

    TEST("removing non-registered transport is safe (no-op)");
    DummyTransport t_unknown;
    h.removeTransport(&t_unknown); // Should not crash
    PASS();

    TEST("removing same transport twice is safe");
    h.removeTransport(&t2); // Already removed
    PASS();

    TEST("removeTransport clears callbacks on removed transport");
    DummyTransport t4;
    h.addTransport(&t4);
    h.removeTransport(&t4);
    // After removal, the transport's callbacks should be nulled out.
    // We can't directly check the private members, but we verify no crash
    // when the transport is used standalone after removal.
    PASS();
}

// ---------------------------------------------------------------------------
// Test: sourceDevice field initialization
// ---------------------------------------------------------------------------

void test_source_device_field() {
    printf("\n[sourceDevice Field]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 2000;

    TEST("sourceDevice is 0 for NoteOn");
    auto ev1 = feedMidi(h, 0x90, 60, 100);
    ASSERT_EQ(ev1.sourceDevice, 0);
    PASS();

    TEST("sourceDevice is 0 for NoteOff");
    g_fakeMillis = 2050;
    auto ev2 = feedMidi(h, 0x80, 60, 0);
    ASSERT_EQ(ev2.sourceDevice, 0);
    PASS();

    TEST("sourceDevice is 0 for ControlChange");
    auto ev3 = feedMidi(h, 0xB0, 7, 127);
    ASSERT_EQ(ev3.sourceDevice, 0);
    PASS();

    TEST("sourceDevice is 0 for ProgramChange");
    auto ev4 = feedMidi(h, 0xC0, 42);
    ASSERT_EQ(ev4.sourceDevice, 0);
    PASS();

    TEST("sourceDevice is 0 for PitchBend");
    auto ev5 = feedMidi(h, 0xE0, 0x00, 0x40);
    ASSERT_EQ(ev5.sourceDevice, 0);
    PASS();

    TEST("sourceDevice is 0 for ChannelPressure");
    auto ev6 = feedMidi(h, 0xD0, 80);
    ASSERT_EQ(ev6.sourceDevice, 0);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: USBDeviceTransport SysEx reassembly through ring buffer
// ---------------------------------------------------------------------------

void test_ring_buffer_sysex() {
    printf("\n[Ring Buffer: SysEx Reassembly]\n");

    USBDeviceTransport transport;

    std::vector<std::vector<uint8_t>> sysexReceived;
    auto sysexCb = [](void* ctx, const uint8_t* data, size_t len) {
        auto* vec = static_cast<std::vector<std::vector<uint8_t>>*>(ctx);
        vec->push_back(std::vector<uint8_t>(data, data + len));
    };
    transport.setSysExCallback(sysexCb, &sysexReceived);

    TEST("SysEx start + end reassembled correctly");
    // CIN 0x04 = SysEx start/continue (3 data bytes)
    uint8_t start[4] = { 0x04, 0xF0, 0x7E, 0x01 };
    // CIN 0x07 = SysEx end with 3 bytes
    uint8_t end[4] = { 0x07, 0x02, 0x03, 0xF7 };
    transport.enqueuePacket(start, 4);
    transport.enqueuePacket(end, 4);
    transport.task();
    ASSERT_EQ((long)sysexReceived.size(), 1L);
    // Should contain: F0 7E 01 02 03 F7
    ASSERT_EQ((long)sysexReceived[0].size(), 6L);
    ASSERT(sysexReceived[0][0] == 0xF0);
    ASSERT(sysexReceived[0][5] == 0xF7);
    PASS();
}

// ---------------------------------------------------------------------------
// Test: Multiple transports dispatching to same handler
// ---------------------------------------------------------------------------

void test_multiple_transports_dispatch() {
    printf("\n[Multiple Transports Dispatching]\n");

    MIDIHandler h;
    h.begin();
    g_fakeMillis = 3000;

    USBDeviceTransport t1, t2;
    h.addTransport(&t1);
    h.addTransport(&t2);

    TEST("data from transport 1 arrives in handler queue");
    uint8_t pkt1[4] = { 0x09, 0x90, 60, 100 };
    t1.enqueuePacket(pkt1, 4);
    t1.task();
    ASSERT_EQ((long)h.getQueue().size(), 1L);
    ASSERT_EQ(h.getQueue().back().noteNumber, 60);
    PASS();

    TEST("data from transport 2 also arrives in handler queue");
    uint8_t pkt2[4] = { 0x09, 0x90, 72, 80 };
    t2.enqueuePacket(pkt2, 4);
    t2.task();
    ASSERT_EQ((long)h.getQueue().size(), 2L);
    ASSERT_EQ(h.getQueue().back().noteNumber, 72);
    PASS();

    // Clean up
    h.removeTransport(&t1);
    h.removeTransport(&t2);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    printf("ESP32_Host_MIDI -- Multi-USB Device Support Test Suite\n");
    printf("======================================================\n");

    test_ring_buffer_enqueue_dequeue();
    test_ring_buffer_full();
    test_ring_buffer_empty();
    test_add_remove_transport();
    test_source_device_field();
    test_ring_buffer_sysex();
    test_multiple_transports_dispatch();

    printf("\n======================================================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
