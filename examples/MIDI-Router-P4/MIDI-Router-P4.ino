// ESP32_Host_MIDI / MIDI-Router-P4
// Selective MIDI routing with USB Host and BLE at the same time, on the ESP32-P4.
//
// - USB Host:   reads from a USB MIDI controller (keyboard, wind controller)
// - BLE Server: accepts a connection from a synth app (iPad, phone)
// - BLE Client: connects out to a BLE MIDI foot controller
//
// All input is processed and forwarded ONLY to the BLE Server (the synth app),
// using the per-transport send API (source tracking + targeted send). No echo
// back to the input devices.
//
// Why the P4 and not the S3 for this: the ESP32-S3 shares one USB PHY between
// USB-OTG and the radio, so USB Host and BLE cannot run together there (the USB
// host goes silent once BLE comes up). The P4 has no native Bluetooth: BLE runs
// on an ESP32-C6 co-processor over SDIO (ESP-Hosted), so nothing contends for the
// USB PHY and USB Host + BLE coexist. For an S3, route USB Host to a DIN-5 output
// instead (see the MIDI-Router example).
//
// USB device note: the P4 USB Host is High-Speed. A Full-Speed device behind a
// hub needs a Transaction Translator, which the ESP-IDF host stack does not
// implement, so connect a High-Speed device or one directly to the host port
// (not through a hub) for it to enumerate.
//
// Board: ESP32-P4. Serial (UART) 115200 stays readable while USB Host runs.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>
#include <BLEConnection.h>
#include <BLEClientConnection.h>

USBConnection usbHost;               // controller on the USB host port
BLEConnection bleSynth;              // synth app connects to us (BLE Server)
BLEClientConnection footController;  // we connect out to the foot controller

static uint64_t lastEventIndex = 0;   // 0 = nothing seen yet (index starts at 1)

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("MIDI Router (P4): USB Host + BLE -> synth");

    usbHost.begin();
    midiHandler.addTransport(&usbHost);

    bleSynth.begin("ESP32 MIDI Router");
    midiHandler.addTransport(&bleSynth);

    footController.setName("Foot Controller");
    footController.begin();           // scans for a BLE MIDI peripheral
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

    // Forward new input to the synth. Skip events from the synth itself (no echo).
    const auto& queue = midiHandler.getQueue();
    for (const auto& event : queue) {
        if (event.index <= lastEventIndex) continue;
        lastEventIndex = event.index;

        if (!event.source || event.source == synth) continue;

        uint8_t ch = event.channel0 + 1;   // send API is 1-based
        switch (event.statusCode) {
            case MIDI_NOTE_ON:
                midiHandler.sendNoteOn(ch, event.noteNumber, event.velocity7, synth);
                break;
            case MIDI_NOTE_OFF:
                midiHandler.sendNoteOff(ch, event.noteNumber, event.velocity7, synth);
                break;
            case MIDI_CONTROL_CHANGE:
                // Foot controller CCs are remapped to channel 16.
                if (event.source == &footController) {
                    midiHandler.sendControlChange(16, event.noteNumber, event.velocity7, synth);
                } else {
                    midiHandler.sendControlChange(ch, event.noteNumber, event.velocity7, synth);
                }
                break;
            case MIDI_PROGRAM_CHANGE:
                midiHandler.sendProgramChange(ch, event.noteNumber, synth);
                break;
            case MIDI_PITCH_BEND:
                midiHandler.sendPitchBend(ch, (int)event.pitchBend14 - 8192, synth);
                break;
            default:
                break;
        }
    }
}
