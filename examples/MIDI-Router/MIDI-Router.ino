// ESP32_Host_MIDI / MIDI-Router
// Selective MIDI routing between transports on the ESP32-S3.
//
// - USB Host: reads from a USB MIDI controller (keyboard, wind controller)
// - UART / DIN-5: forwards to classic 5-pin MIDI gear (synth, sound module)
//
// Input from USB is processed and forwarded ONLY to the DIN-5 output, using the
// per-transport send API (source tracking + targeted send).
//
// Why these two transports: USB Host and UART are independent peripherals, so
// they run together with no contention. On the ESP32-S3, USB Host cannot share
// the chip with a radio transport (BLE / Wi-Fi / ESP-NOW): the USB PHY is shared
// with the radio. For USB Host + BLE together, use the ESP32-P4 (see the
// MIDI-Router-P4 example).
//
// Board: ESP32-S3 (USB Host requires S3/S2/P4). Serial 115200.
// DIN-5: a MIDI output circuit on UART1 (see the UART-MIDI-Basic example wiring).

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>
#include <UARTConnection.h>

USBConnection usbHost;    // controller plugged into the OTG port
UARTConnection dinOut;    // DIN-5 output on UART1

static const int DIN_RX_PIN = 44;   // adjust to your board
static const int DIN_TX_PIN = 43;

static uint64_t lastEventIndex = 0;   // 0 = nothing seen yet (index starts at 1)

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("MIDI Router (S3): USB Host -> DIN-5");

    usbHost.begin();
    midiHandler.addTransport(&usbHost);

    dinOut.begin(Serial1, DIN_RX_PIN, DIN_TX_PIN);
    midiHandler.addTransport(&dinOut);

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

    // Route every new event that came from USB out to the DIN-5 port.
    const auto& queue = midiHandler.getQueue();
    for (const auto& event : queue) {
        if (event.index <= lastEventIndex) continue;
        lastEventIndex = event.index;

        // Only forward what the USB controller sent; never echo DIN back to USB.
        if (event.source != &usbHost) continue;

        uint8_t ch = event.channel0 + 1;   // send API is 1-based
        switch (event.statusCode) {
            case MIDI_NOTE_ON:
                midiHandler.sendNoteOn(ch, event.noteNumber, event.velocity7, &dinOut);
                break;
            case MIDI_NOTE_OFF:
                midiHandler.sendNoteOff(ch, event.noteNumber, event.velocity7, &dinOut);
                break;
            case MIDI_CONTROL_CHANGE:
                midiHandler.sendControlChange(ch, event.noteNumber, event.velocity7, &dinOut);
                break;
            case MIDI_PROGRAM_CHANGE:
                midiHandler.sendProgramChange(ch, event.noteNumber, &dinOut);
                break;
            case MIDI_PITCH_BEND:
                midiHandler.sendPitchBend(ch, (int)event.pitchBend14 - 8192, &dinOut);
                break;
            default:
                break;
        }
    }
}
