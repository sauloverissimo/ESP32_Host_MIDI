// ESP32_Host_MIDI / T-Display-S3-Piano-Flow
// 25-key piano visualizer driven by gingoduino's midi2flow engine.
//
// Source-agnostic: a USB MIDI 1.0 keyboard, a USB MIDI 2.0 device, or a MIDI 2.0
// UMP source all flow through the same interpreter. USBMIDI2Connection handles
// both MIDI 1.0 (Alt 0, bytes) and MIDI 2.0 (Alt 1, UMP) on the host port;
// BLEConnection adds wireless MIDI 1.0. GingoFlowAdapter normalizes every source
// to UMP and feeds midi2flow, which names the chord (with inversion) and tracks
// note durations. The existing PianoDisplay + PCM5102A synth are reused.
//
// Requires: LovyanGFX + Gingoduino 0.6.0+ (with midi2flow).
// Arduino IDE: Board T-Display-S3 (ESP32-S3) | Partition: Huge App (3MB) | Serial 115200

#include <Arduino.h>
#include <USBMIDI2Connection.h>   // USB host: MIDI 1.0 (Alt 0) + MIDI 2.0/UMP (Alt 1)
#include <BLEConnection.h>        // BLE MIDI 1.0
#include "GingoFlowAdapter.h"
#include "PianoDisplay.h"
#include "SynthEngine.h"
#include "mapping.h"

// ── Global instances ──────────────────────────────────────────────────────────
USBMIDI2Connection usb;      // host port: keyboards (MIDI 1.0) and the MIDI 2.0 gabarito
BLEConnection      ble;      // wireless MIDI 1.0 (iPad/iPhone)
SynthEngine        synth;
GingoFlowAdapter   adapter;  // any source -> UMP -> midi2flow

// ── Display state ─────────────────────────────────────────────────────────────
static bool activeNotes[128]     = {};
static bool prevActiveNotes[128] = {};
static PianoInfo info = {};

// Build the display info from the flow's state (held notes + named chord + duration).
static void buildInfo() {
    char noteStr[64] = "";
    uint8_t count = 0;
    int root = -1;
    for (int n = 0; n < 128; ++n) {
        if (!adapter.state().active((uint8_t)n)) continue;
        if (root < 0) root = n;
        if (count) strncat(noteStr, "  ", sizeof(noteStr) - strlen(noteStr) - 1);
        char buf[8];
        gingo::GingoNote gn = gingo::GingoNote::fromMIDI((uint8_t)n);
        snprintf(buf, sizeof buf, "%s%d", gn.name(), (int)gingo::GingoNote::octaveFromMIDI((uint8_t)n));
        strncat(noteStr, buf, sizeof(noteStr) - strlen(noteStr) - 1);
        ++count;
    }
    info.noteCount = count;
    info.rootMidi  = (root < 0) ? 0 : root;
    strncpy(info.noteStr, noteStr, sizeof(info.noteStr) - 1);
    info.noteStr[sizeof(info.noteStr) - 1] = '\0';

    // chordName carries the flow's name WITH inversion (e.g. "CM/E").
    strncpy(info.chordName, adapter.state().chordText(), sizeof(info.chordName) - 1);
    info.chordName[sizeof(info.chordName) - 1] = '\0';

    // duration of the last closed note, shown where the full name used to be.
    uint32_t dur = adapter.state().lastDurationMs();
    if (dur) snprintf(info.chordFullName, sizeof(info.chordFullName), "%lu ms", (unsigned long)dur);
    else     info.chordFullName[0] = '\0';
    info.formula[0] = '\0';
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    piano.init();
    piano.render(activeNotes, info);

    adapter.attach(usb);
    adapter.attach(ble);
    usb.begin();
    ble.begin("ESP32 Piano Flow");

    synth.begin();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
static uint32_t lastFrameUs = 0;
static const uint32_t FRAME_US = 33000;  // ~30 fps

void loop() {
    usb.task();
    ble.task();
    adapter.poll();

    for (int n = 0; n < 128; ++n) activeNotes[n] = adapter.state().active((uint8_t)n);

    if (memcmp(activeNotes, prevActiveNotes, sizeof(activeNotes)) != 0) {
        for (int n = 0; n < 128; ++n) {
            if (activeNotes[n] && !prevActiveNotes[n]) synth.noteOn(n, 100);
            if (!activeNotes[n] && prevActiveNotes[n]) synth.noteOff(n);
        }
        memcpy(prevActiveNotes, activeNotes, sizeof(activeNotes));
        buildInfo();
    }

    uint32_t now = micros();
    if (now - lastFrameUs >= FRAME_US) {
        lastFrameUs = now;
        piano.render(activeNotes, info);
    }
}
