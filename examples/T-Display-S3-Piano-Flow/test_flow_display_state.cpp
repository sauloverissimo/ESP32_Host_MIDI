// Host test for FlowDisplayState: feed MIDI 2.0 note-ons via the flow and check
// the display-facing state (held notes + chord name with inversion).
// Build (from this dir, point -I at the gingoduino repo root):
//   g++ -std=c++11 -Wall -Wextra -I <gingoduino> -I . test_flow_display_state.cpp -o /tmp/t_fs && /tmp/t_fs
#include <cstdio>
#include <cstring>
#include <initializer_list>   // required for the range-for over { ... } below

#include "src/Gingoduino.h"
#include "src/GingoNote.cpp"
#include "src/GingoInterval.cpp"
#include "src/GingoChord.cpp"
#include "FlowDisplayState.h"

using namespace gingo;

static uint32_t on0(uint8_t n){ return ((uint32_t)0x4<<28)|((uint32_t)0x9<<20)|((uint32_t)n<<8); }
static uint32_t off0(uint8_t n){ return ((uint32_t)0x4<<28)|((uint32_t)0x8<<20)|((uint32_t)n<<8); }

static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){ printf("FAIL %s\n",m); ++fails;} }while(0)

int main() {
    // C major triad (root position): C4 E4 G4
    {
        FlowDisplayState st;
        uint32_t t = 0;
        for (uint8_t n : {60,64,67}) { uint32_t w[2]={on0(n),0x80000000u}; st.ingest(w,2,t); }
        st.poll();
        CHECK(st.active(60) && st.active(64) && st.active(67), "held C4 E4 G4");
        CHECK(strcmp(st.chordText(),"CM")==0, "root chord = CM");
    }
    // C major 1st inversion: E4 G4 C5 -> CM/E
    {
        FlowDisplayState st;
        uint32_t t = 0;
        for (uint8_t n : {64,67,72}) { uint32_t w[2]={on0(n),0x80000000u}; st.ingest(w,2,t); }
        st.poll();
        CHECK(strcmp(st.chordText(),"CM/E")==0, "1st inversion = CM/E");
    }
    // duration: note on at t=0, off at t=500 -> lastDurationMs == 500
    {
        FlowDisplayState st;
        uint32_t won[2]={on0(60),0x80000000u};  st.ingest(won,2,0);   st.poll();
        uint32_t wof[2]={off0(60),0u};          st.ingest(wof,2,500); st.poll();
        CHECK(!st.active(60), "note released");
        CHECK(st.lastDurationMs()==500, "duration 500ms");
    }

    printf(fails ? "FAILED %d\n" : "ALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
