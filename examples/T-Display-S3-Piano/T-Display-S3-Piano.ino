// Example: Piano Visualizer + Synthesizer
//
// Designed for MIDI-25 keyboards (e.g. Arturia Minilab 25).
// Shows all 25 keys (C to C, 2 octaves) on the ST7789 display of the T-Display S3.
// Highlights pressed keys in real time and plays the notes via the onboard PCM5102A DAC.
// Displays note names, chord identification (Gingoduino), and music theory.
//
// Anti-tearing: full-screen sprite (double-buffered in PSRAM).
// Auto-view: automatically adjusts the visible octave range.
//
// Controls:
//   Button 1 (GPIO  0): shift view one octave down
//   Button 2 (GPIO 14): shift view one octave up
//
// Dependencies:
//   - ESP32_Host_MIDI (this library)
//   - Gingoduino v0.2.2+  (https://github.com/sauloverissimo/gingoduino)
//   - LovyanGFX

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <GingoAdapter.h>
#include "PianoDisplay.h"
#include "SynthEngine.h"
#include "mapping.h"

using namespace gingoduino;

// ── Global instances ──────────────────────────────────────────────────────────
SynthEngine synth;

// ── Active notes state ────────────────────────────────────────────────────────
static bool activeNotes[128]     = {};
static bool prevActiveNotes[128] = {};
static int  lastEventIndex       = -1;
static bool viewCalibrated       = false;

// ── Persistent music analysis ─────────────────────────────────────────────────
static PianoInfo info = {};

// ── Button debounce ───────────────────────────────────────────────────────────
static unsigned long btn1Last = 0, btn2Last = 0;
static const unsigned long DEBOUNCE_MS = 200;

// ── Chord type → full name (active language) ─────────────────────────────────
static const char* chordTypeName(const char* chordName) {
    if (!chordName || !*chordName) return "";
    const char* t = chordName + 1;
    if (*t == '#' || *t == 'b') t++;
    struct { const char* code; const char* en; } MAP[] = {
        {"M",       "Major"},       {"m",       "Minor"},
        {"7",       "Dominant 7"},  {"7M",      "Major 7"},
        {"m7",      "Minor 7"},     {"dim",     "Diminished"},
        {"dim7",    "Dim. 7"},      {"m7(b5)",  "Half-dim."},
        {"+",       "Augmented"},   {"sus2",    "Sus 2"},
        {"sus4",    "Sus 4"},       {"M9",      "Major 9"},
        {"m9",      "Minor 9"},     {nullptr,   nullptr}
    };
    for (int i = 0; MAP[i].code; i++) if (strcmp(t, MAP[i].code) == 0) return MAP[i].en;
    return t;
}

// ── Gingoduino analysis ───────────────────────────────────────────────────────
static void analyzeNotes() {
    uint8_t midiNotes[GingoAdapter::MAX_CHORD_NOTES];
    uint8_t count = 0;
    for (int n = 0; n < 128 && count < GingoAdapter::MAX_CHORD_NOTES; n++) {
        if (activeNotes[n]) midiNotes[count++] = (uint8_t)n;
    }

    memset(&info, 0, sizeof(info));
    info.noteCount = count;

    if (count == 0) return;

    GingoNote gingoNotes[GingoAdapter::MAX_CHORD_NOTES];
    uint8_t n = GingoAdapter::midiToGingoNotes(midiNotes, count, gingoNotes);

    info.rootMidi  = midiNotes[0];
    int8_t rootOctave = GingoNote::octaveFromMIDI(midiNotes[0]);
    info.rootFreq  = gingoNotes[0].frequency(rootOctave);

    char noteStr[64] = "";
    for (uint8_t i = 0; i < n; i++) {
        if (i) strcat(noteStr, "  ");
        char buf[8];
        int8_t oct = GingoNote::octaveFromMIDI(midiNotes[i]);
        snprintf(buf, sizeof(buf), "%s%d", gingoNotes[i].name(), (int)oct);
        strcat(noteStr, buf);
    }
    strncpy(info.noteStr, noteStr, sizeof(info.noteStr) - 1);

    if (n == 1) return;

    if (n == 2) {
        GingoInterval iv(gingoNotes[0], gingoNotes[1]);
        iv.label(info.intervalLabel, sizeof(info.intervalLabel));
        iv.fullName(info.intervalName, sizeof(info.intervalName));
        return;
    }

    if (GingoChord::identify(gingoNotes, n, info.chordName, sizeof(info.chordName))) {
        const char* typeEn = chordTypeName(info.chordName);
        snprintf(info.chordFullName, sizeof(info.chordFullName),
                 "%s %s", gingoNotes[0].name(), typeEn);
    } else {
        strncpy(info.chordName, "?", sizeof(info.chordName));
    }

    char formula[64] = "";
    for (uint8_t i = 0; i < n; i++) {
        GingoInterval iv(gingoNotes[0], gingoNotes[i]);
        char lbl[8];
        iv.label(lbl, sizeof(lbl));
        if (i) strcat(formula, "  ");
        strcat(formula, lbl);
    }
    strncpy(info.formula, formula, sizeof(info.formula) - 1);
}

// ── Process new MIDI events from queue ────────────────────────────────────────
static void processQueue(const std::deque<MIDIEventData>& queue) {
    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        if (ev.note >= 0 && ev.note < 128) {
            if (ev.status == "NoteOn" && ev.velocity > 0) {
                activeNotes[ev.note] = true;
                synth.noteOn((uint8_t)ev.note, (uint8_t)ev.velocity);
                // One-time view calibration on the very first note
                if (!viewCalibrated) {
                    viewCalibrated = true;
                    piano.setViewStart(ev.note);
                }
                Serial.printf("[MIDI] NoteOn  %s (MIDI %d) vel=%d  view=%d\n",
                              ev.noteOctave.c_str(), ev.note, ev.velocity,
                              piano.getViewStart());
            } else if (ev.status == "NoteOff" ||
                      (ev.status == "NoteOn" && ev.velocity == 0)) {
                activeNotes[ev.note] = false;
                synth.noteOff((uint8_t)ev.note);
                Serial.printf("[MIDI] NoteOff %s (MIDI %d)\n",
                              ev.noteOctave.c_str(), ev.note);
            }
        }
        lastEventIndex = ev.index;
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[Piano] Starting...");

    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);

    piano.init();
    piano.render(activeNotes, info);

    MIDIHandlerConfig cfg;
    cfg.maxEvents       = 256;
    cfg.chordTimeWindow = 0;
    cfg.bleName         = "ESP32 Piano";
    midiHandler.begin(cfg);

    synth.begin();

    Serial.printf("[Piano] View: MIDI %d-%d (%s%d to %s%d)\n",
        VIEW_DEFAULT, VIEW_DEFAULT + KEYS_SPAN - 1,
        NOTE_NAMES[VIEW_DEFAULT % 12], (VIEW_DEFAULT / 12) - 1,
        NOTE_NAMES[(VIEW_DEFAULT + KEYS_SPAN - 1) % 12],
        ((VIEW_DEFAULT + KEYS_SPAN - 1) / 12) - 1);
    Serial.println("[Piano] Ready. Auto-view enabled.");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
static uint32_t lastFrameUs = 0;
static const uint32_t FRAME_US = 16000;  // ~60 fps

void loop() {
    midiHandler.task();

    processQueue(midiHandler.getQueue());

    // Re-analyse only when note set changes
    if (memcmp(activeNotes, prevActiveNotes, sizeof(activeNotes)) != 0) {
        memcpy(prevActiveNotes, activeNotes, sizeof(activeNotes));
        analyzeNotes();
    }

    // Display update — rate-limited, entire frame via single pushSprite
    uint32_t now = micros();
    if (now - lastFrameUs >= FRAME_US) {
        lastFrameUs = now;
        piano.render(activeNotes, info);
    }

    // Periodic validation: compare our tracking with MIDIHandler's authoritative state
    static unsigned long lastValidMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastValidMs > 2000) {
        lastValidMs = nowMs;
        int ourCount = 0;
        for (int n = 0; n < 128; n++) if (activeNotes[n]) ourCount++;
        int handlerCount = (int)midiHandler.getActiveNotesCount();
        if (ourCount != handlerCount) {
            Serial.printf("[VALID] MISMATCH! ours=%d handler=%d\n", ourCount, handlerCount);
        }
    }

    // Buttons — shift octave view
    if (digitalRead(PIN_BUTTON_1) == LOW && nowMs - btn1Last > DEBOUNCE_MS) {
        btn1Last = nowMs;
        piano.shiftOctave(-12);
        Serial.printf("[Piano] Manual shift: view starts at MIDI %d\n",
                      piano.getViewStart());
    }
    if (digitalRead(PIN_BUTTON_2) == LOW && nowMs - btn2Last > DEBOUNCE_MS) {
        btn2Last = nowMs;
        piano.shiftOctave(+12);
        Serial.printf("[Piano] Manual shift: view starts at MIDI %d\n",
                      piano.getViewStart());
    }
}
