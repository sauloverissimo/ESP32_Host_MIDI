// Native test for the BLE-MIDI packet splitter (src/BleMidiPacket.h).
// Builds with: g++ -std=c++17 -Iextras/tests/stub -Isrc -o test_ble_midi_split \
//   extras/tests/test_ble_midi_split.cpp

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "BleMidiPacket.h"

static int g_passed = 0;
static int g_failed = 0;

struct Collected {
    std::vector<std::vector<uint8_t>> msgs;
};

static void collect(void* ctx, const uint8_t* m, size_t n) {
    static_cast<Collected*>(ctx)->msgs.emplace_back(m, m + n);
}

static Collected split(const std::vector<uint8_t>& pkt) {
    Collected c;
    bleMidiSplitMessages(pkt.data(), pkt.size(), collect, &c);
    return c;
}

static void expect(const char* name, const Collected& got,
                   const std::vector<std::vector<uint8_t>>& want) {
    bool ok = (got.msgs.size() == want.size());
    for (size_t i = 0; ok && i < want.size(); i++) ok = (got.msgs[i] == want[i]);
    if (ok) {
        g_passed++;
    } else {
        g_failed++;
        printf("  FAIL: %s (got %zu msgs, want %zu)\n", name, got.msgs.size(), want.size());
    }
}

int main() {
    printf("[BLE-MIDI splitter]\n");

    // Single Note On.
    expect("single note on",
           split({0x80, 0x80, 0x90, 0x40, 0x64}),
           {{0x90, 0x40, 0x64}});

    // Batched Note On + Note Off, each preceded by its own timestamp.
    expect("batched note on + note off",
           split({0x80, 0x80, 0x90, 0x40, 0x64, 0x81, 0x80, 0x40, 0x00}),
           {{0x90, 0x40, 0x64}, {0x80, 0x40, 0x00}});

    // Running status: second event omits the status byte.
    expect("running status",
           split({0x80, 0x80, 0x90, 0x40, 0x64, 0x81, 0x42, 0x50}),
           {{0x90, 0x40, 0x64}, {0x90, 0x42, 0x50}});

    // Control Change (two data bytes).
    expect("control change",
           split({0x80, 0x80, 0xB0, 0x07, 0x7F}),
           {{0xB0, 0x07, 0x7F}});

    // Program Change (one data byte).
    expect("program change",
           split({0x80, 0x80, 0xC0, 0x05}),
           {{0xC0, 0x05}});

    // Batched CC + PC (mixed data-byte counts).
    expect("batched cc + pc",
           split({0x80, 0x80, 0xB0, 0x07, 0x7F, 0x81, 0xC0, 0x05}),
           {{0xB0, 0x07, 0x7F}, {0xC0, 0x05}});

    // Real-time (System Real-Time, zero data bytes).
    expect("real-time clock",
           split({0x80, 0x80, 0xF8}),
           {{0xF8}});

    // SysEx: remainder handed over as one blob (no reframing).
    expect("sysex passthrough",
           split({0x80, 0x80, 0xF0, 0x7D, 0x01, 0x02, 0xF7}),
           {{0xF0, 0x7D, 0x01, 0x02, 0xF7}});

    // Truncated final event (Note On missing its velocity) is dropped.
    expect("truncated dropped",
           split({0x80, 0x80, 0x90, 0x40}),
           {});

    // A complete event followed by a truncated one: keep the complete, drop the rest.
    expect("complete then truncated",
           split({0x80, 0x80, 0x90, 0x40, 0x64, 0x81, 0xB0, 0x07}),
           {{0x90, 0x40, 0x64}});

    // Too short to carry MIDI: nothing emitted (matches the pre-split behavior).
    expect("too short", split({0x80, 0x80}), {});

    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
