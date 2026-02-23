// Example: BLE MIDI Sender with Didactic Display
//
// Sends pre-programmed musical sequences via Bluetooth Low Energy to a
// BLE MIDI receiver (e.g. T-Display-S3-BLE-Receiver or any BLE MIDI app).
//
// The display shows didactically:
//   - BLE connection status
//   - Sequence name and current step
//   - Note names being sent (human-readable)
//   - Raw MIDI bytes in hex (educational)
//   - Mini piano with active key highlights
//
// Controls:
//   Button 1 (GPIO 0):  Cycle through sequences
//   Button 2 (GPIO 14): Play / Stop
//
// Portability:
//   The BLE Client and Sequence Player sections are independent of the display.
//   To adapt for ESP32 without a display, simply remove the SenderDisplay calls.
//   The MusicSequences.h file has no hardware dependencies.
//
// Dependencies: LovyanGFX (for display only), ESP32 BLE library

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "SenderDisplay.h"
#include "MusicSequences.h"
#include "mapping.h"

// ═══════════════════════════════════════════════════════════════════════════════
// Section 1: BLE MIDI Client
// ═══════════════════════════════════════════════════════════════════════════════
// This section handles BLE scanning, connecting, and sending MIDI packets.
// Portable to any ESP32 board — no display dependency.

// BLE MIDI standard UUIDs
#define BLE_MIDI_SERVICE_UUID        "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define BLE_MIDI_CHARACTERISTIC_UUID "7772E5DB-3868-4112-A1A9-F2669D106BF3"

// The receiver's BLE name (must match cfg.bleName in the receiver sketch)
static const char* TARGET_NAME = "ESP32 BLE Piano";

// BLE state
static BLEAdvertisedDevice* targetDevice = nullptr;
static BLEClient*           pClient      = nullptr;
static BLERemoteCharacteristic* pChar    = nullptr;
static volatile bool bleConnected = false;
static volatile bool bleScanning  = false;

// ── BLE Client Callbacks ──────────────────────────────────────────────────────

class ClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient*) override {
        bleConnected = true;
        Serial.println("[BLE] Connected!");
    }
    void onDisconnect(BLEClient*) override {
        bleConnected = false;
        pChar = nullptr;
        Serial.println("[BLE] Disconnected.");
    }
};

static ClientCallbacks clientCbs;

// ── BLE Scan Callbacks ────────────────────────────────────────────────────────

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (advertisedDevice.haveName() &&
            strcmp(advertisedDevice.getName().c_str(), TARGET_NAME) == 0) {
            Serial.printf("[BLE] Found '%s'!\n", TARGET_NAME);
            if (targetDevice) delete targetDevice;
            targetDevice = new BLEAdvertisedDevice(advertisedDevice);
            advertisedDevice.getScan()->stop();
        }
    }
};

// ── BLE Functions ─────────────────────────────────────────────────────────────

static void startScan() {
    if (bleScanning) return;
    Serial.println("[BLE] Scanning...");
    bleScanning = true;
    BLEScan* pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new ScanCallbacks(), true);
    pScan->setActiveScan(true);
    pScan->start(5, false);  // 5 seconds, non-blocking
    bleScanning = false;
}

static bool connectToServer() {
    if (!targetDevice) return false;
    Serial.printf("[BLE] Connecting to '%s'...\n", TARGET_NAME);

    if (!pClient) {
        pClient = BLEDevice::createClient();
        pClient->setCallbacks(&clientCbs);
    }

    if (!pClient->connect(targetDevice)) {
        Serial.println("[BLE] Connection failed.");
        return false;
    }

    BLERemoteService* svc = pClient->getService(BLEUUID(BLE_MIDI_SERVICE_UUID));
    if (!svc) {
        Serial.println("[BLE] MIDI service not found.");
        pClient->disconnect();
        return false;
    }

    pChar = svc->getCharacteristic(BLEUUID(BLE_MIDI_CHARACTERISTIC_UUID));
    if (!pChar) {
        Serial.println("[BLE] MIDI characteristic not found.");
        pClient->disconnect();
        return false;
    }

    Serial.println("[BLE] Ready to send MIDI!");
    return true;
}

// Send a BLE MIDI packet with correct BLE MIDI 1.0 header.
// Format: [header][timestamp][status][data1][data2]
// The receiver (BLEConnection) strips the first 2 bytes and processes raw MIDI.
static bool sendBLEMidi(uint8_t status, uint8_t data1, uint8_t data2) {
    if (!pChar || !bleConnected) return false;

    uint32_t ts = (uint32_t)millis();
    uint8_t packet[5];
    packet[0] = 0x80 | ((ts >> 7) & 0x3F);  // BLE MIDI header
    packet[1] = 0x80 | (ts & 0x7F);          // BLE MIDI timestamp
    packet[2] = status;                        // MIDI status byte
    packet[3] = data1;                         // MIDI data byte 1
    packet[4] = data2;                         // MIDI data byte 2

    pChar->writeValue(packet, 5, false);  // write without response (low latency)
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Section 2: Sequence Player
// ═══════════════════════════════════════════════════════════════════════════════
// State machine that plays pre-programmed sequences using millis() (no delay).
// Portable to any Arduino-compatible board — no display or BLE dependency.

static int  currentSeq    = 0;
static int  currentStep   = 0;
static bool playing       = false;
static bool noteHeld      = false;   // true while notes are sounding
static unsigned long stepStartMs = 0;

// Track which notes are currently active (for display)
static bool senderNotes[128] = {};

// Last sent status/note/vel (for display)
static uint8_t lastStatus   = 0;
static uint8_t lastNote     = 0;
static uint8_t lastVelocity = 0;

static void sendCurrentStepOn() {
    const NoteStep& step = ALL_SEQUENCES[currentSeq].steps[currentStep];
    for (int i = 0; i < step.count; i++) {
        sendBLEMidi(0x90, step.notes[i], step.velocity);
        senderNotes[step.notes[i]] = true;
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
        sendBLEMidi(0x80, step.notes[i], 0);
        senderNotes[step.notes[i]] = false;
    }
    lastStatus   = 0x80;
    lastNote     = step.notes[0];
    lastVelocity = 0;
}

static void stopAll() {
    // Send NoteOff for any held notes
    for (int n = 0; n < 128; n++) {
        if (senderNotes[n]) {
            sendBLEMidi(0x80, n, 0);
            senderNotes[n] = false;
        }
    }
    playing  = false;
    noteHeld = false;
    lastStatus = 0;
    currentStep = 0;
}

static void playerTick(unsigned long now) {
    if (!playing || !bleConnected) return;

    const Sequence& seq = ALL_SEQUENCES[currentSeq];
    const NoteStep& step = seq.steps[currentStep];

    if (!noteHeld) {
        // Send NoteOn
        sendCurrentStepOn();
        noteHeld = true;
        stepStartMs = now;

    } else if (now - stepStartMs >= step.durationMs) {
        // Duration elapsed — send NoteOff
        sendCurrentStepOff();
        noteHeld = false;
        stepStartMs = now;

        // Advance to next step (after pause)
    } else {
        return;  // still holding note
    }

    // Handle pause and advance (when noteHeld just became false)
    if (!noteHeld && (now - stepStartMs >= step.pauseMs)) {
        currentStep++;
        if (currentStep >= seq.stepCount) {
            if (seq.loop) {
                currentStep = 0;
            } else {
                stopAll();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Section 3: Display (T-Display-S3 specific — remove for headless boards)
// ═══════════════════════════════════════════════════════════════════════════════

static SenderInfo buildDisplayInfo() {
    SenderInfo info = {};
    info.bleConnected   = bleConnected;
    info.bleScanning    = bleScanning;
    info.sequenceName   = ALL_SEQUENCES[currentSeq].name;
    info.currentStep    = currentStep;
    info.totalSteps     = ALL_SEQUENCES[currentSeq].stepCount;
    info.playing        = playing;
    info.activeNotes    = senderNotes;
    info.currentStatus  = lastStatus;
    info.currentVelocity = lastVelocity;

    // Copy current step notes
    if (playing && noteHeld) {
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
    Serial.println("=== BLE MIDI Sender ===");

    // Board hardware
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);

    // Display
    senderDisplay.init();

    // BLE
    BLEDevice::init(String("BLE MIDI Sender"));
    Serial.printf("Scanning for '%s'...\n", TARGET_NAME);
    startScan();
}

static uint32_t lastFrameMs     = 0;
static uint32_t lastReconnectMs = 0;
static uint32_t btn1Last = 0, btn2Last = 0;

void loop() {
    uint32_t now = millis();

    // ── BLE reconnection ──────────────────────────────────────────────────────
    if (!bleConnected && !bleScanning && (now - lastReconnectMs > 3000)) {
        lastReconnectMs = now;
        if (targetDevice) {
            connectToServer();
        } else {
            startScan();
        }
    }

    // ── Button 1: Next sequence ───────────────────────────────────────────────
    if (digitalRead(PIN_BUTTON_1) == LOW && (now - btn1Last > 250)) {
        btn1Last = now;
        if (playing) stopAll();
        currentSeq = (currentSeq + 1) % NUM_SEQUENCES;
        currentStep = 0;
        lastStatus = 0;
        Serial.printf("[SEQ] -> %s\n", ALL_SEQUENCES[currentSeq].name);
    }

    // ── Button 2: Play / Stop ─────────────────────────────────────────────────
    if (digitalRead(PIN_BUTTON_2) == LOW && (now - btn2Last > 250)) {
        btn2Last = now;
        if (playing) {
            stopAll();
            Serial.println("[SEQ] Stopped.");
        } else if (bleConnected) {
            playing     = true;
            currentStep = 0;
            noteHeld    = false;
            stepStartMs = now;
            lastStatus  = 0;
            Serial.printf("[SEQ] Playing: %s\n", ALL_SEQUENCES[currentSeq].name);
        } else {
            Serial.println("[SEQ] Cannot play — BLE not connected.");
        }
    }

    // ── Sequence player ───────────────────────────────────────────────────────
    playerTick(now);

    // ── Display update (~30 fps) ──────────────────────────────────────────────
    if (now - lastFrameMs >= 33) {
        lastFrameMs = now;
        SenderInfo info = buildDisplayInfo();
        senderDisplay.render(info);
    }
}
