// Example: ESP-NOW MIDI Jam — Bidirectional Wireless MIDI
//
// Upload the SAME sketch to two T-Display-S3 boards.
// Each board plays pre-programmed sequences and broadcasts them via ESP-NOW.
// The other board receives and displays the notes on a shared piano visualizer.
//
// The display shows:
//   - Cyan keys:    notes this board is playing (local)
//   - Magenta keys: notes the other board is playing (remote)
//   - Green keys:   both boards playing the same note
//   - ESP-NOW peer status and local MAC address
//   - Sequence name, note names, raw MIDI bytes (educational)
//
// Controls:
//   Button 1 (GPIO 0):  Cycle through sequences
//   Button 2 (GPIO 14): Play / Stop
//
// Each board auto-selects a different starting sequence based on its MAC
// address, so they don't play the same thing by default.
//
// Portability:
//   The ESP-NOW and Sequence Player sections are independent of the display.
//   To adapt for ESP32 without a display, simply remove the JamDisplay calls.
//   Works on any ESP32 variant with WiFi (ESP32, S2, S3, C3, C6).
//
// Dependencies: LovyanGFX (for display only), ESP32 WiFi/ESP-NOW

#include <Arduino.h>
#include "ESP32_Host_MIDI.h"
#include "ESPNowConnection.h"

#include "JamDisplay.h"
#include "MusicSequences.h"
#include "mapping.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: ESP-NOW Transport
// ═══════════════════════════════════════════════════════════════════════════════
// Broadcast MIDI — no pairing needed. Ultra-low latency (~1-5ms).
// Portable to any ESP32 board — no display dependency.

ESPNowConnection espNow;

// Remote notes state (filled by ESP-NOW receive callback)
static bool remoteNotes[128] = {};
static volatile unsigned long lastRemoteMs = 0;

// ESP-NOW receive callback — runs on WiFi task, keep it fast
static void onEspNowData(void* ctx, const uint8_t* data, size_t length) {
    if (length < 2) return;
    uint8_t status = data[0] & 0xF0;

    if (status == 0x90 && length >= 3 && data[2] > 0) {
        remoteNotes[data[1]] = true;
    } else if (status == 0x80 && length >= 3) {
        remoteNotes[data[1]] = false;
    } else if (status == 0x90 && length >= 3 && data[2] == 0) {
        // NoteOn with velocity 0 = NoteOff (standard MIDI)
        remoteNotes[data[1]] = false;
    }

    lastRemoteMs = millis();
}

// Send a MIDI message via ESP-NOW broadcast
static bool sendMidi(uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t packet[3] = { status, data1, data2 };
    return espNow.sendMidiMessage(packet, 3);
}

// Local MAC (for display and auto-ID)
static uint8_t localMAC[6] = {};

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Sequence Player
// ═══════════════════════════════════════════════════════════════════════════════
// State machine that plays pre-programmed sequences using millis() (no delay).
// Portable to any Arduino-compatible board — no display or network dependency.

static int  currentSeq    = 0;
static int  currentStep   = 0;
static bool playing       = false;

// Player states: IDLE → NOTE_ON → PAUSE → advance → NOTE_ON ...
enum PlayerPhase { PH_IDLE, PH_NOTE_ON, PH_PAUSE };
static PlayerPhase playerPhase = PH_IDLE;
static unsigned long phaseStartMs = 0;

// Track which notes are currently active (for display)
static bool localNotes[128] = {};

// Last sent status/note/vel (for display)
static uint8_t lastStatus   = 0;
static uint8_t lastNote     = 0;
static uint8_t lastVelocity = 0;

static void sendCurrentStepOn() {
    const NoteStep& step = ALL_SEQUENCES[currentSeq].steps[currentStep];
    for (int i = 0; i < step.count; i++) {
        sendMidi(0x90, step.notes[i], step.velocity);
        localNotes[step.notes[i]] = true;
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
        sendMidi(0x80, step.notes[i], 0);
        localNotes[step.notes[i]] = false;
    }
    lastStatus   = 0x80;
    lastNote     = step.notes[0];
    lastVelocity = 0;
}

static void stopAll() {
    for (int n = 0; n < 128; n++) {
        if (localNotes[n]) {
            sendMidi(0x80, n, 0);
            localNotes[n] = false;
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
        // Start playing: send first NoteOn
        sendCurrentStepOn();
        playerPhase  = PH_NOTE_ON;
        phaseStartMs = now;
        break;

    case PH_NOTE_ON:
        // Holding note — wait for duration to elapse
        if (now - phaseStartMs >= step.durationMs) {
            sendCurrentStepOff();
            playerPhase  = PH_PAUSE;
            phaseStartMs = now;
        }
        break;

    case PH_PAUSE:
        // Silence between notes — wait for pause to elapse
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

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Display (T-Display-S3 specific — remove for headless boards)
// ═══════════════════════════════════════════════════════════════════════════════

static JamInfo buildDisplayInfo() {
    JamInfo info = {};
    info.peerActive     = (millis() - lastRemoteMs < 3000);
    info.localMAC[0]    = localMAC[3];
    info.localMAC[1]    = localMAC[4];
    info.localMAC[2]    = localMAC[5];
    info.sequenceName   = ALL_SEQUENCES[currentSeq].name;
    info.currentStep    = currentStep;
    info.totalSteps     = ALL_SEQUENCES[currentSeq].stepCount;
    info.playing        = playing;
    info.localNotes     = localNotes;
    info.remoteNotes    = remoteNotes;
    info.currentStatus  = lastStatus;
    info.currentVelocity = lastVelocity;

    // Copy current step notes for display
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
    Serial.println("=== ESP-NOW MIDI Jam ===");

    // Board hardware
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);

    // Display
    jamDisplay.init();

    // ESP-NOW
    espNow.getLocalMAC(localMAC);
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  localMAC[0], localMAC[1], localMAC[2],
                  localMAC[3], localMAC[4], localMAC[5]);

    if (!espNow.begin()) {
        Serial.println("ESP-NOW init failed!");
        while (true) delay(1000);
    }
    espNow.setMidiCallback(onEspNowData, nullptr);
    Serial.println("ESP-NOW initialized (broadcast mode).");

    // Auto-select starting sequence based on MAC (so two boards differ)
    currentSeq = localMAC[5] % NUM_SEQUENCES;
    Serial.printf("Starting sequence: %s (auto-selected from MAC)\n",
                  ALL_SEQUENCES[currentSeq].name);
}

static uint32_t lastFrameMs = 0;
static uint32_t btn1Last = 0, btn2Last = 0;

void loop() {
    uint32_t now = millis();

    // ── ESP-NOW task (process received messages) ─────────────────────────────
    espNow.task();

    // ── Button 1: Next sequence ──────────────────────────────────────────────
    if (digitalRead(PIN_BUTTON_1) == LOW && (now - btn1Last > 250)) {
        btn1Last = now;
        if (playing) stopAll();
        currentSeq = (currentSeq + 1) % NUM_SEQUENCES;
        currentStep = 0;
        lastStatus = 0;
        Serial.printf("[SEQ] -> %s\n", ALL_SEQUENCES[currentSeq].name);
    }

    // ── Button 2: Play / Stop ────────────────────────────────────────────────
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
        }
    }

    // ── Sequence player ──────────────────────────────────────────────────────
    playerTick(now);

    // ── Display update (~30 fps) ─────────────────────────────────────────────
    if (now - lastFrameMs >= 33) {
        lastFrameMs = now;
        JamInfo info = buildDisplayInfo();
        jamDisplay.render(info);
    }
}
