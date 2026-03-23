// MIDI Router Example
//
// Demonstrates selective MIDI routing between transports:
// - USB Host IN: reads from a MIDI controller (e.g., wind controller, keyboard)
// - BLE Server: accepts connection from a synth app (iPad, phone)
// - BLE Client: connects to a foot controller (BLE MIDI peripheral)
//
// All input is processed and forwarded ONLY to the BLE Server (synth).
// No echo back to input devices.
//
// Hardware: ESP32-S3 with USB Host support
// Connections:
//   USB Host port -> MIDI controller
//   BLE -> foot controller (peripheral) + synth app (central connects to us)

#include <ESP32_Host_MIDI.h>
#include "BLEClientConnection.h"

// BLE Client: connects to a foot controller (BLE MIDI peripheral)
BLEClientConnection footController;

void setup() {
    Serial.begin(115200);
    Serial.println("MIDI Router starting...");

    // Configure handler
    MIDIHandlerConfig config;
    config.bleName = "ESP32 MIDI Router";  // BLE Server name (synth connects here)
    config.maxEvents = 50;
    midiHandler.begin(config);
    // Built-in transports registered automatically:
    //   USB Host (index 0)
    //   BLE Server (index 1)

    // Add BLE Client for the foot controller
    footController.setName("Foot Controller");
    footController.begin();  // starts scanning for BLE MIDI peripherals
    midiHandler.addTransport(&footController);

    // Print registered transports
    Serial.println("Transports:");
    for (int i = 0; i < midiHandler.getTransportCount(); i++) {
        Serial.printf("  [%d] %s\n", i, midiHandler.getTransport(i)->name());
    }
}

void loop() {
    midiHandler.task();

    // Get the synth output transport (BLE Server)
    MIDITransport* synth = midiHandler.getBLETransport();

    // Process events and route selectively
    auto& queue = midiHandler.getQueue();
    for (const auto& event : queue) {
        // Skip events with no source or from the synth itself (no echo)
        if (!event.source || event.source == synth) continue;

        // Route based on message type
        switch (event.statusCode) {
            case MIDI_NOTE_ON:
                midiHandler.sendNoteOn(event.channel0 + 1, event.noteNumber,
                                       event.velocity7, synth);
                break;

            case MIDI_NOTE_OFF:
                midiHandler.sendNoteOff(event.channel0 + 1, event.noteNumber,
                                        event.velocity7, synth);
                break;

            case MIDI_CONTROL_CHANGE:
                // Example: remap foot controller CCs to channel 16
                if (event.source == &footController) {
                    midiHandler.sendControlChange(16, event.noteNumber,
                                                   event.velocity7, synth);
                } else {
                    midiHandler.sendControlChange(event.channel0 + 1, event.noteNumber,
                                                   event.velocity7, synth);
                }
                break;

            case MIDI_PITCH_BEND:
                midiHandler.sendPitchBend(event.channel0 + 1,
                                           event.pitchBend14 - 8192, synth);
                break;

            case MIDI_PROGRAM_CHANGE:
                midiHandler.sendProgramChange(event.channel0 + 1,
                                               event.noteNumber, synth);
                break;

            default:
                break;
        }
    }

    midiHandler.clearQueue();
}
