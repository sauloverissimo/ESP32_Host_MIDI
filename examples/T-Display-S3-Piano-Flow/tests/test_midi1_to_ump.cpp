// Host test for Midi1ToUmp: MIDI 1.0 channel-voice bytestream -> UMP MT 0x2.
// Build: g++ -std=c++11 -Wall -Wextra -I . test_midi1_to_ump.cpp -o /tmp/t_m1 && /tmp/t_m1
#include <cstdio>
#include <cstdint>
#include <vector>
#include "../Midi1ToUmp.h"

static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){ printf("FAIL %s\n",m); ++fails;} }while(0)

int main() {
    std::vector<uint32_t> out;
    Midi1ToUmp conv(0, [&](uint32_t w0, uint32_t w1){ out.push_back(w0); out.push_back(w1); });

    // Note On ch0 C4 vel100  ->  UMP MT2: 0x2 group0 0x9 ch0 note vel
    const uint8_t on[] = {0x90, 60, 100};
    conv.feed(on, 3);
    CHECK(out.size()==2, "note on -> 1 UMP (2 words)");
    CHECK((out[0]>>28)==0x2u, "MT 0x2");
    CHECK(((out[0]>>20)&0xF)==0x9u, "status note on");
    CHECK(((out[0]>>8)&0x7F)==60u, "note 60");
    CHECK((out[0]&0x7F)==100u, "vel 100");

    // running status: another note without repeating 0x90
    out.clear();
    const uint8_t rs[] = {62, 80};
    conv.feed(rs, 2);
    CHECK(out.size()==2 && ((out[0]>>8)&0x7F)==62u, "running status note 62");

    // one-data-byte status: program change ch0 prog 5
    out.clear();
    const uint8_t pc[] = {0xC0, 5};
    conv.feed(pc, 2);
    CHECK(out.size()==2 && ((out[0]>>20)&0xF)==0xCu && ((out[0]>>8)&0x7F)==5u, "program change 1 data byte");

    // realtime byte interleaved (0xF8) must be ignored, not break running status
    out.clear();
    const uint8_t rt[] = {0x90, 67, 0xF8, 90};   // note 67, clock, vel 90
    conv.feed(rt, 4);
    CHECK(out.size()==2 && ((out[0]>>8)&0x7F)==67u && (out[0]&0x7F)==90u, "realtime ignored mid-message");

    printf(fails ? "FAILED %d\n" : "ALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
