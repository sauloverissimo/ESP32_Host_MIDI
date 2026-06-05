#ifndef FLOW_DISPLAY_STATE_H
#define FLOW_DISPLAY_STATE_H

// Display-facing state derived from midi2flow. Subscribes to the flow's logical
// stream and maintains exactly what the piano display needs: which notes are
// held (activeNotes), the current chord named with inversion (via
// GingoChord::identifyFromMidi), and the last closed note's duration.
// Source-agnostic: it only sees UMP that some adapter ingested.

#include <cstdint>
#include <cstdio>
#include <Gingoduino.h>   // gingo::GingoFlow, gingo::GingoChord, gingo::GingoFlowEvent
                          // (Arduino: lib src/ is on the include path; host tests
                          //  add -I <gingoduino>/src so this also resolves)

class FlowDisplayState {
public:
    // One processed event, kept for the scrolling log view. Records the kind,
    // the note/CC index, and the interpreter's indices (event order, note
    // pairing, chord grouping) so the log shows the live interpretation.
    struct LogEntry {
        uint8_t  kind;        // gingo::EventKind
        uint8_t  note;        // note number / CC index
        uint8_t  group;       // UMP group 0..15 (separates sources)
        uint8_t  channel;     // 0..15
        uint16_t velocity;    // 16-bit velocity (MIDI 1.0 is upscaled 7->16)
        uint32_t evIdx;       // idx.event
        uint32_t noteIdx;     // idx.note (pairs on/off)
        uint32_t chordIdx;    // idx.chord (simultaneous group)
        uint32_t durationMs;  // set on the paired note-off
    };
    static const uint8_t LOG_CAP = 48;

    FlowDisplayState() {
        flow_.onEvent([this](const gingo::GingoFlowEvent& e){ onEvent(e); });
    }

    bool ingest(const uint32_t* w, uint8_t n, uint32_t now) { return flow_.ingest(w, n, now); }
    void poll() { while (flow_.process()) {} rebuildChord(); }

    bool        active(uint8_t n) const { return n < 128 && active_[n]; }
    const char* chordText() const       { return chordText_; }
    uint32_t    lastDurationMs() const  { return lastDur_; }

    // Scrolling event log (oldest retained at index 0).
    uint16_t        logCount() const          { return logCount_; }
    const LogEntry& logAt(uint16_t i) const   { return log_[(logHead_ + i) % LOG_CAP]; }

private:
    void onEvent(const gingo::GingoFlowEvent& e) {
        if (e.kind == gingo::NOTE_ON  && e.note < 128) active_[e.note] = true;
        if (e.kind == gingo::NOTE_OFF && e.note < 128) {
            active_[e.note] = false;
            if (e.idx.note) lastDur_ = e.durationMs;   // paired note-off carries the duration
        }
        // Append every processed event to the ring (overwrites the oldest).
        LogEntry le{ (uint8_t)e.kind, e.note, e.group, e.channel, e.velocity,
                     e.idx.event, e.idx.note, e.idx.chord, e.durationMs };
        log_[(logHead_ + logCount_) % LOG_CAP] = le;
        if (logCount_ < LOG_CAP) logCount_++;
        else                     logHead_ = (logHead_ + 1) % LOG_CAP;
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

    gingo::GingoFlow<128> flow_;
    bool     active_[128] = {};
    char     chordText_[28] = {};
    uint32_t lastDur_ = 0;

    LogEntry log_[LOG_CAP] = {};
    uint16_t logHead_  = 0;   // index of the oldest retained entry
    uint16_t logCount_ = 0;
};

#endif  // FLOW_DISPLAY_STATE_H
