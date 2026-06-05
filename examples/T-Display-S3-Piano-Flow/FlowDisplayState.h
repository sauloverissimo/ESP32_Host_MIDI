#ifndef FLOW_DISPLAY_STATE_H
#define FLOW_DISPLAY_STATE_H

// Display-facing state derived from midi2flow. Subscribes to the flow's logical
// stream and maintains exactly what the piano display needs: which notes are
// held (activeNotes), the current chord named with inversion (via
// GingoChord::identifyFromMidi), and the last closed note's duration.
// Source-agnostic: it only sees UMP that some adapter ingested.

#include <cstdint>
#include <cstdio>
#include "src/Gingoduino.h"   // gingo::Midi2Flow, gingo::GingoChord, gingo::Event

class FlowDisplayState {
public:
    FlowDisplayState() {
        flow_.onEvent([this](const gingo::Event& e){ onEvent(e); });
    }

    bool ingest(const uint32_t* w, uint8_t n, uint32_t now) { return flow_.ingest(w, n, now); }
    void poll() { while (flow_.process()) {} rebuildChord(); }

    bool        active(uint8_t n) const { return n < 128 && active_[n]; }
    const char* chordText() const       { return chordText_; }
    uint32_t    lastDurationMs() const  { return lastDur_; }

private:
    void onEvent(const gingo::Event& e) {
        if (e.kind == gingo::NOTE_ON  && e.note < 128) active_[e.note] = true;
        if (e.kind == gingo::NOTE_OFF && e.note < 128) {
            active_[e.note] = false;
            if (e.idx.note) lastDur_ = e.durationMs;   // paired note-off carries the duration
        }
    }

    void rebuildChord() {
        uint8_t held[16];
        uint8_t c = 0;
        for (uint16_t n = 0; n < 128 && c < 16; ++n) if (active_[n]) held[c++] = (uint8_t)n;
        if (c < 2) { chordText_[0] = '\0'; return; }

        static const char* NN[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        char nm[24];
        uint8_t bass = 0, inv = 0;
        if (gingo::GingoChord::identifyFromMidi(held, c, nm, sizeof nm, &bass, &inv)) {
            if (inv == 0) std::snprintf(chordText_, sizeof chordText_, "%s", nm);
            else          std::snprintf(chordText_, sizeof chordText_, "%s/%s", nm, NN[bass]);
        } else {
            chordText_[0] = '\0';
        }
    }

    gingo::Midi2Flow<128> flow_;
    bool     active_[128] = {};
    char     chordText_[28] = {};
    uint32_t lastDur_ = 0;
};

#endif  // FLOW_DISPLAY_STATE_H
