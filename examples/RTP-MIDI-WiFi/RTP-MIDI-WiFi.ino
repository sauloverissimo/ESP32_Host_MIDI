// Example: RTP-MIDI WiFi with Didactic Display
//
// Connects the ESP32 to WiFi and exposes it as an RTP-MIDI (AppleMIDI) device.
// macOS/iOS auto-discovers it in "Audio MIDI Setup → Network" and it appears
// as a MIDI port in Logic Pro, GarageBand, Ableton and any CoreMIDI app
// — no driver, no USB cable required.
//
// The display shows didactically:
//   - WiFi IP address and RTP-MIDI peer count
//   - Sequence name and current step
//   - Note names being sent (human-readable)
//   - Raw MIDI bytes in hex (educational)
//   - Mini piano with active key highlights
//
// Controls:
//   Button 1 (GPIO 0):  Cycle through sequences
//   Button 2 (GPIO 14): Play / Stop
//
// Requirements:
//   1. Install "AppleMIDI" library by lathoub (v3.x) via Arduino Library Manager.
//   2. Install "MIDI Library" by Francois Best via Arduino Library Manager.
//   3. Fill in your WiFi credentials in mapping.h (WIFI_SSID, WIFI_PASS).
//
// Dependencies: LovyanGFX (for display), AppleMIDI + MIDI Library

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>

#include "../../src/RTPMIDIConnection.h"  // or "RTPMIDIConnection.h" when library is installed
                                          // Requires: lathoub/Arduino-AppleMIDI-Library v3.x

#include "RTPDisplay.h"
#include "MusicSequences.h"
#include "mapping.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: WiFi + RTP-MIDI Setup
// ═══════════════════════════════════════════════════════════════════════════════

// ---- RTP-MIDI device name (shown in macOS/iOS Audio MIDI Setup) -------
#define DEVICE_NAME  "ESP32 MIDI"
// -----------------------------------------------------------------------

RTPMIDIConnection rtpMIDI;

// ── WiFi helper ─────────────────────────────────────────────────────────────

static bool connectWiFi() {
    Serial.print("Connecting to " WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            Serial.println(" TIMEOUT");
            return false;
        }
        delay(250);
        Serial.print('.');
    }
    Serial.println();
    Serial.print("  IP: "); Serial.println(WiFi.localIP());
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Sequence Player
// ═══════════════════════════════════════════════════════════════════════════════
// State machine that plays pre-programmed sequences using millis() (no delay).
// Sends via midiHandler which routes to all registered transports (RTP-MIDI).

static int  currentSeq    = 0;
static int  currentStep   = 0;
static bool playing       = false;

enum PlayerPhase { PH_IDLE, PH_NOTE_ON, PH_PAUSE };
static PlayerPhase playerPhase = PH_IDLE;
static unsigned long phaseStartMs = 0;

// Combined active notes: player + received (for display)
static bool activeNotes[128] = {};

// Last sent info for display
static uint8_t lastStatus   = 0;
static uint8_t lastNote     = 0;
static uint8_t lastVelocity = 0;
static int     lastEventIndex = -1;

static void sendCurrentStepOn() {
    const NoteStep& step = ALL_SEQUENCES[currentSeq].steps[currentStep];
    for (int i = 0; i < step.count; i++) {
        uint8_t msg[3] = {
            (uint8_t)(0x90),            // NoteOn, channel 1 (0-based)
            step.notes[i],
            step.velocity
        };
        rtpMIDI.sendMidiMessage(msg, 3);
        activeNotes[step.notes[i]] = true;
        Serial.printf("  NoteOn:  %s%d (MIDI %d, vel %d)\n",
                      midiNoteName(step.notes[i]), midiNoteOctave(step.notes[i]),
                      step.notes[i], step.velocity);
    }
    lastStatus   = 0x90;
    lastNote     = step.notes[0];
    lastVelocity = step.velocity;
}

static void sendCurrentStepOff() {
    const NoteStep& step = ALL_SEQUENCES[currentSeq].steps[currentStep];
    for (int i = 0; i < step.count; i++) {
        uint8_t msg[3] = {
            (uint8_t)(0x80),            // NoteOff, channel 1 (0-based)
            step.notes[i],
            (uint8_t)0
        };
        rtpMIDI.sendMidiMessage(msg, 3);
        activeNotes[step.notes[i]] = false;
    }
    lastStatus   = 0x80;
    lastNote     = step.notes[0];
    lastVelocity = 0;
}

static void stopAll() {
    for (int n = 0; n < 128; n++) {
        if (activeNotes[n]) {
            uint8_t msg[3] = { (uint8_t)(0x80), (uint8_t)n, (uint8_t)0 };
            rtpMIDI.sendMidiMessage(msg, 3);
            activeNotes[n] = false;
        }
    }
    playing     = false;
    playerPhase = PH_IDLE;
    lastStatus  = 0;
    currentStep = 0;
}

static void playerTick(unsigned long now) {
    if (!playing) return;

    const Sequence& seq = ALL_SEQUENCES[currentSeq];
    const NoteStep& step = seq.steps[currentStep];

    switch (playerPhase) {
    case PH_IDLE:
        sendCurrentStepOn();
        playerPhase  = PH_NOTE_ON;
        phaseStartMs = now;
        break;

    case PH_NOTE_ON:
        if (now - phaseStartMs >= step.durationMs) {
            sendCurrentStepOff();
            playerPhase  = PH_PAUSE;
            phaseStartMs = now;
        }
        break;

    case PH_PAUSE:
        if (now - phaseStartMs >= step.pauseMs) {
            currentStep++;
            if (currentStep >= seq.stepCount) {
                if (seq.loop) {
                    currentStep = 0;
                } else {
                    stopAll();
                    return;
                }
            }
            sendCurrentStepOn();
            playerPhase  = PH_NOTE_ON;
            phaseStartMs = now;
        }
        break;
    }
}

// ── Process received notes (bidirectional: USB keyboard / DAW) ───────────────

static void processReceivedNotes() {
    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;

        if (ev.note < 0 || ev.note > 127) continue;

        if (ev.status == "NoteOn" && ev.velocity > 0) {
            activeNotes[ev.note] = true;
        } else if (ev.status == "NoteOff" || (ev.status == "NoteOn" && ev.velocity == 0)) {
            activeNotes[ev.note] = false;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Display
// ═══════════════════════════════════════════════════════════════════════════════

static RTPInfo buildDisplayInfo() {
    RTPInfo info = {};
    info.wifiConnected  = (WiFi.status() == WL_CONNECTED);

    if (info.wifiConnected) {
        IPAddress ip = WiFi.localIP();
        snprintf(info.ipAddress, sizeof(info.ipAddress), "%d.%d.%d.%d",
                 ip[0], ip[1], ip[2], ip[3]);
    } else {
        strncpy(info.ipAddress, "---", sizeof(info.ipAddress));
    }

    info.peerCount      = rtpMIDI.connectedCount();
    info.rtpActive      = info.peerCount > 0;
    info.sequenceName   = ALL_SEQUENCES[currentSeq].name;
    info.currentStep    = currentStep;
    info.totalSteps     = ALL_SEQUENCES[currentSeq].stepCount;
    info.playing        = playing;
    info.activeNotes    = activeNotes;
    info.currentStatus  = lastStatus;
    info.currentVelocity = lastVelocity;

    // Current step notes for display
    if (playing && playerPhase == PH_NOTE_ON) {
        const NoteStep& step = ALL_SEQUENCES[currentSeq].steps[currentStep];
        memcpy(info.currentNotes, step.notes, sizeof(step.notes));
        info.currentNoteCount = step.count;
        info.currentVelocity  = step.velocity;
        info.currentStatus    = 0x90;
    } else if (lastStatus == 0x80) {
        info.currentNotes[0]  = lastNote;
        info.currentNoteCount = 1;
        info.currentStatus    = 0x80;
    }

    return info;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Section 4: Setup & Loop
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("=== RTP-MIDI WiFi + Display ===");

    // Board hardware
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);

    // Display
    rtpDisplay.init();

    // WiFi
    if (!connectWiFi()) {
        Serial.println("WiFi failed. Halting.");
        while (true) delay(1000);
    }

    // RTP-MIDI
    if (rtpMIDI.begin(DEVICE_NAME)) {
        Serial.println("RTP-MIDI session open on port " + String(RTP_MIDI_PORT));
        Serial.println("  Open 'Audio MIDI Setup -> Network' on macOS to connect.");
    } else {
        Serial.println("RTP-MIDI begin() failed.");
    }

    // MIDIHandler (for receiving USB MIDI if available)
    midiHandler.addTransport(&rtpMIDI);
    MIDIHandlerConfig cfg;
    cfg.maxEvents = 30;
    midiHandler.begin(cfg);

    Serial.println("Ready. Connect from macOS Audio MIDI Setup.");
}

static uint32_t lastFrameMs = 0;
static uint32_t btn1Last = 0, btn2Last = 0;

void loop() {
    uint32_t now = millis();

    // Process MIDI from all transports (USB + RTP)
    midiHandler.task();

    // Process received notes for display
    processReceivedNotes();

    // ── Button 1: Next sequence ─────────────────────────────────────────────
    if (digitalRead(PIN_BUTTON_1) == LOW && (now - btn1Last > 250)) {
        btn1Last = now;
        if (playing) stopAll();
        currentSeq = (currentSeq + 1) % NUM_SEQUENCES;
        currentStep = 0;
        lastStatus = 0;
        Serial.printf("[SEQ] -> %s\n", ALL_SEQUENCES[currentSeq].name);
    }

    // ── Button 2: Play / Stop ───────────────────────────────────────────────
    if (digitalRead(PIN_BUTTON_2) == LOW && (now - btn2Last > 250)) {
        btn2Last = now;
        if (playing) {
            stopAll();
            Serial.println("[SEQ] Stopped.");
        } else {
            playing     = true;
            currentStep = 0;
            playerPhase = PH_IDLE;
            lastStatus  = 0;
            Serial.printf("[SEQ] Playing: %s\n", ALL_SEQUENCES[currentSeq].name);
            if (!rtpMIDI.isConnected()) Serial.println("[SEQ] (No RTP peer — display only)");
        }
    }

    // ── Sequence player ─────────────────────────────────────────────────────
    playerTick(now);

    // ── Display update (~30 fps) ────────────────────────────────────────────
    if (now - lastFrameMs >= 33) {
        lastFrameMs = now;
        RTPInfo info = buildDisplayInfo();
        rtpDisplay.render(info);
    }
}
