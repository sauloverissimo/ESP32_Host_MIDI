// Host test for FlowDisplayState: feed MIDI 2.0 note-ons via the flow and check
// the display-facing state (held notes + chord name with inversion).
// Build (from this dir; -I the gingoduino repo root for the .cpp includes and
// its src/ so FlowDisplayState.h's <Gingoduino.h> resolves like in Arduino):
//   g++ -std=c++11 -Wall -Wextra -I <gingoduino> -I <gingoduino>/src -I . test_flow_display_state.cpp -o /tmp/t_fs && /tmp/t_fs
#include <cstdio>
#include <cstring>
#include <initializer_list>   // required for the range-for over { ... } below

#include "src/Gingoduino.h"
#include "src/GingoNote.cpp"
#include "src/GingoInterval.cpp"
#include "src/GingoChord.cpp"
#include "../FlowDisplayState.h"

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
    // event log: a C major triad logs three NOTE_ON, one chord group, rising
    // event order; the paired note-off logs a NOTE_OFF carrying the duration.
    {
        FlowDisplayState st;
        for (uint8_t n : {60,64,67}) { uint32_t w[2]={on0(n),0x80000000u}; st.ingest(w,2,0); }
        st.poll();
        CHECK(st.logCount()==3, "log has 3 note-ons");
        CHECK(st.logAt(0).kind==gingo::NOTE_ON && st.logAt(0).note==60, "log[0] ON 60");
        CHECK(st.logAt(1).note==64 && st.logAt(2).note==67, "log[1..2] = 64,67");
        CHECK(st.logAt(0).velocity==0x8000 && st.logAt(0).group==0 && st.logAt(0).channel==0,
              "log captures velocity/group/channel");
        CHECK(st.logAt(0).chordIdx==st.logAt(2).chordIdx, "same chord group");
        CHECK(st.logAt(0).evIdx < st.logAt(2).evIdx, "event order rises");

        uint32_t wof[2]={off0(60),0u}; st.ingest(wof,2,400); st.poll();
        CHECK(st.logCount()==4, "log has 4 events");
        CHECK(st.logAt(3).kind==gingo::NOTE_OFF && st.logAt(3).note==60, "log[3] OFF 60");
        CHECK(st.logAt(3).noteIdx==st.logAt(0).noteIdx, "off pairs the on (noteIdx)");
        CHECK(st.logAt(3).durationMs==400, "off carries duration 400ms");
    }
    // log ring overflow: more than LOG_CAP events keep only the last LOG_CAP,
    // and the oldest retained advances (a window over the newest events).
    {
        FlowDisplayState st;
        const int N = FlowDisplayState::LOG_CAP + 10;
        for (int i = 0; i < N; ++i) {
            uint8_t n = (uint8_t)(40 + (i % 30));
            uint32_t w[2]={on0(n),0x80000000u}; st.ingest(w,2,(uint32_t)i); st.poll();
        }
        CHECK(st.logCount()==FlowDisplayState::LOG_CAP, "log capped at LOG_CAP");
        CHECK(st.logAt(st.logCount()-1).evIdx > st.logAt(0).evIdx, "window holds newest");
    }

    printf(fails ? "FAILED %d\n" : "ALL TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
