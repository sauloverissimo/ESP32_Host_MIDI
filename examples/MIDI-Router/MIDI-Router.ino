// ESP32_Host_MIDI / MIDI-Router
// Selective MIDI routing between transports.
//
// - USB Host: reads from a MIDI controller (wind controller, keyboard)
// - BLE Server: accepts connection from a synth app (iPad, phone)
// - BLE Client: connects to a foot controller (BLE MIDI peripheral)
//
// All input is processed and forwarded ONLY to the BLE Server (synth).
// No echo back to input devices.
//
// Requires: ESP32-S3 (or S2/P4) for USB Host, BLE-capable chip for BLE.
// Arduino IDE: Board ESP32-S3 | USB Mode: default | Serial 115200

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>
#include <BLEConnection.h>
#include <BLEClientConnection.h>

USBConnection usbHost;               // MIDI controller plugged into the OTG port
BLEConnection bleSynth;              // synth app connects to us (BLE Server)
BLEClientConnection footController;  // we connect to the foot controller (BLE Client)

// Last processed event index, used to detect new events in the queue.
static int lastEventIndex = -1;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("MIDI Router - starting");

    usbHost.begin();
    midiHandler.addTransport(&usbHost);

    bleSynth.begin("ESP32 MIDI Router");
    midiHandler.addTransport(&bleSynth);

    footController.setName("Foot Controller");
    footController.begin();  // starts scanning for BLE MIDI peripherals
    midiHandler.addTransport(&footController);

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 50;
    midiHandler.begin(cfg);

    Serial.println("Transports:");
    for (int i = 0; i < midiHandler.getTransportCount(); i++) {
        Serial.printf("  [%d] %s\n", i, midiHandler.getTransport(i)->name());
    }
}

void loop() {
    midiHandler.task();

    MIDITransport* synth = &bleSynth;

    // Process new events and route selectively.
    const auto& queue = midiHandler.getQueue();
    for (const auto& event : queue) {
        if (event.index <= lastEventIndex) continue;
        lastEventIndex = event.index;

        // Skip events with no source or from the synth itself (no echo).
        if (!event.source || event.source == synth) continue;

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
                // Example: remap foot controller CCs to channel 16.
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
                                          (int)event.pitchBend14 - 8192, synth);
                break;

            case MIDI_PROGRAM_CHANGE:
                midiHandler.sendProgramChange(event.channel0 + 1,
                                              event.noteNumber, synth);
                break;

            default:
                break;
        }
    }
}
