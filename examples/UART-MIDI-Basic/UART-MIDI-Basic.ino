// UART-MIDI-Basic — ESP32_Host_MIDI example
//
// Receives MIDI messages from a DIN-5 (5-pin) MIDI cable via UART and
// prints them to the Serial monitor (115200 baud).
// Optionally sends a NoteOn/NoteOff pair every 2 seconds over MIDI OUT.
//
// Hardware (MIDI IN):
//   DIN-5 pin 5 ──── 220Ω ──── optocoupler anode  (e.g. 6N138 or TLP2361)
//   DIN-5 pin 2 ──── GND
//   Optocoupler output ──── MIDI_RX_PIN
//
// Hardware (MIDI OUT, optional):
//   MIDI_TX_PIN ──── 220Ω ──── DIN-5 pin 5
//   3.3V        ──── 220Ω ──── DIN-5 pin 4
//   DIN-5 pin 2 ──── GND
//
// Adjust the pin defines below to match your board.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include "../../src/UARTConnection.h"  // or just "UARTConnection.h" if library is installed

// ---- Pin configuration ------------------------------------------------
#define MIDI_RX_PIN   18   // GPIO connected to optocoupler output (MIDI IN)
#define MIDI_TX_PIN   17   // GPIO connected to MIDI OUT circuit (-1 if unused)
// -----------------------------------------------------------------------

UARTConnection uartMIDI;

// Last processed event index — used to detect new events in the queue.
static int lastEventIndex = -1;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("UART MIDI Basic — starting");

    // Open the MIDI serial port and register as an external transport.
    uartMIDI.begin(Serial1, MIDI_RX_PIN, MIDI_TX_PIN);
    midiHandler.addTransport(&uartMIDI);

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 20;
    midiHandler.begin(cfg);

    Serial.println("Ready. Connect a MIDI device to pin " + String(MIDI_RX_PIN));
}

void loop() {
    midiHandler.task();

    // Process all new events that arrived since last loop iteration.
    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;

        Serial.print("[MIDI] ");
        Serial.print(ev.status.c_str());

        if (ev.status == "NoteOn" || ev.status == "NoteOff") {
            Serial.print("  ch=");   Serial.print(ev.channel);
            Serial.print("  note="); Serial.print(ev.noteOctave.c_str());
            Serial.print("  vel=");  Serial.println(ev.velocity);
        } else if (ev.status == "ControlChange") {
            Serial.print("  ch=");  Serial.print(ev.channel);
            Serial.print("  cc=");  Serial.print(ev.note);
            Serial.print("  val="); Serial.println(ev.velocity);
        } else if (ev.status == "ProgramChange") {
            Serial.print("  ch=");   Serial.print(ev.channel);
            Serial.print("  prog="); Serial.println(ev.velocity);
        } else if (ev.status == "PitchBend") {
            Serial.print("  ch="); Serial.print(ev.channel);
            Serial.print("  pb="); Serial.println(ev.pitchBend);
        } else {
            Serial.println();
        }
    }

    // Optional: send a NoteOn / NoteOff every 2 seconds to test MIDI OUT.
#if MIDI_TX_PIN >= 0
    static unsigned long lastSend = 0;
    static bool noteOn = false;
    if (millis() - lastSend >= 2000) {
        lastSend = millis();
        if (!noteOn) {
            midiHandler.sendNoteOn(1, 60, 100);  // Ch 1, C4, vel 100
            Serial.println("[SEND] NoteOn C4");
        } else {
            midiHandler.sendNoteOff(1, 60, 0);
            Serial.println("[SEND] NoteOff C4");
        }
        noteOn = !noteOn;
    }
#endif
}
