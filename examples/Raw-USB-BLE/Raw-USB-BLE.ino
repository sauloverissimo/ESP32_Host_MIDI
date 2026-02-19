// Example: Raw USB and BLE MIDI Data
// Demonstrates using USBConnection and BLEConnection directly,
// WITHOUT MIDIHandler, for raw MIDI byte access.
//
// This is useful for:
// - Building custom MIDI parsers
// - MIDI routing/forwarding applications
// - Logging raw MIDI traffic for debugging
// - Minimal memory footprint applications

#include <Arduino.h>
#include "USBConnection.h"
#include "BLEConnection.h"

// --- USB: Subclass with override ---

class MyRawUSB : public USBConnection {
public:
    void onMidiDataReceived(const uint8_t* data, size_t length) override {
        // data[0] = Cable Number / Code Index Number (USB-MIDI header)
        // data[1] = MIDI status byte (e.g., 0x90 = NoteOn ch1)
        // data[2] = MIDI data byte 1 (e.g., note number)
        // data[3] = MIDI data byte 2 (e.g., velocity)
        Serial.printf("[USB] CIN:%02X  Status:%02X  Data1:%02X  Data2:%02X\n",
                      data[0], data[1], data[2], data[3]);
    }

    void onDeviceConnected() override {
        Serial.println("[USB] MIDI device connected!");
    }

    void onDeviceDisconnected() override {
        Serial.println("[USB] MIDI device disconnected!");
    }
};

MyRawUSB usbMidi;

// --- BLE: Callback function ---

BLEConnection bleMidi;

void onBleRawData(const uint8_t* data, size_t length) {
    Serial.printf("[BLE] Raw %d bytes:", length);
    for (size_t i = 0; i < length; i++) {
        Serial.printf(" %02X", data[i]);
    }
    Serial.println();
}

// --- Setup & Loop ---

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Raw MIDI Example ===");
    Serial.println("No MIDIHandler — direct access to raw bytes.");
    Serial.println();

    // Initialize USB Host (runs on core 0)
    if (usbMidi.begin()) {
        Serial.println("USB Host initialized. Waiting for MIDI device...");
    } else {
        Serial.println("USB Host init failed: " + usbMidi.getLastError());
    }

    // Initialize BLE MIDI server
    bleMidi.setMidiMessageCallback(onBleRawData);
    bleMidi.begin("ESP32 Raw MIDI");
    Serial.println("BLE MIDI server started. Advertising as 'ESP32 Raw MIDI'.");
    Serial.println();
}

void loop() {
    usbMidi.task();
    bleMidi.task();
    delayMicroseconds(10);
}
