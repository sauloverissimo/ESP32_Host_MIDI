// Example: ESP-NOW Wireless MIDI
// Demonstrates ultra-low latency wireless MIDI between ESP32 devices.
//
// Upload to two ESP32s — both send and receive simultaneously.
// By default uses broadcast (no pairing needed).
//
// Option A: Standalone — raw MIDI bytes via callback (no MIDIHandler).
// Option B: With MIDIHandler — full MIDI parsing, chords, active notes, etc.
//           Uncomment the lines marked "Option B" below.
//
// Latency: ~1-5ms (significantly faster than BLE MIDI).

#include <Arduino.h>
#include "ESP32_Host_MIDI.h"
#include "ESPNowConnection.h"

ESPNowConnection espNow;

// --- ESP-NOW data callback (Option A: standalone) ---

void onEspNowData(void* ctx, const uint8_t* data, size_t length) {
    uint8_t status = data[0] & 0xF0;
    uint8_t channel = (data[0] & 0x0F) + 1;

    if (status == 0x90 && length >= 3) {
        Serial.printf("[ESP-NOW] NoteOn  ch=%d  note=%d  vel=%d\n",
                      channel, data[1], data[2]);
    } else if (status == 0x80 && length >= 3) {
        Serial.printf("[ESP-NOW] NoteOff ch=%d  note=%d  vel=%d\n",
                      channel, data[1], data[2]);
    } else {
        Serial.printf("[ESP-NOW] Raw:");
        for (size_t i = 0; i < length; i++) {
            Serial.printf(" %02X", data[i]);
        }
        Serial.println();
    }
}

// --- Setup & Loop ---

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== ESP-NOW MIDI Example ===");

    // Print this device's MAC address
    uint8_t mac[6];
    espNow.getLocalMAC(mac);
    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Initialize ESP-NOW transport
    if (!espNow.begin()) {
        Serial.println("ESP-NOW init failed!");
        while (true) delay(1000);
    }
    Serial.println("ESP-NOW initialized (broadcast mode).");

    // --- Option A: Standalone (raw callback) ---
    espNow.setMidiCallback(onEspNowData, nullptr);

    // --- Option B: With MIDIHandler (full MIDI parsing) ---
    // midiHandler.addTransport(&espNow);
    // MIDIHandlerConfig cfg;
    // cfg.bleName = "ESP32 MIDI";
    // midiHandler.begin(cfg);

    Serial.println("Sending test NoteOn every 2 seconds...");
    Serial.println();
}

uint8_t noteNum = 60;

void loop() {
    espNow.task();
    // midiHandler.task();  // Option B

    // Send a test NoteOn/NoteOff cycle every 2 seconds
    static unsigned long lastSend = 0;
    static bool noteActive = false;

    if (millis() - lastSend > 2000) {
        lastSend = millis();

        if (noteActive) {
            // Send NoteOff
            uint8_t noteOff[3] = { 0x80, noteNum, 0 };
            espNow.sendMidiMessage(noteOff, 3);
            Serial.printf("Sent NoteOff: note=%d\n", noteNum);
            noteNum = 60 + ((noteNum - 60 + 1) % 12);
        } else {
            // Send NoteOn
            uint8_t noteOn[3] = { 0x90, noteNum, 100 };
            espNow.sendMidiMessage(noteOn, 3);
            Serial.printf("Sent NoteOn:  note=%d\n", noteNum);
        }
        noteActive = !noteActive;
    }
}
