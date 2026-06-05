// ESP32_Host_MIDI / UART-MIDI-Basic
// Receive (and optionally send) MIDI 1.0 over a UART / DIN-5 port.
//
// Requires: none beyond the board.
// Arduino IDE: Board ESP32-S3 (or any ESP32) | Serial 115200
//
// Wiring is in the README. Adjust the pin defines below to match your board.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <UARTConnection.h>

// ---- Pin configuration ------------------------------------------------
#define MIDI_RX_PIN   18   // GPIO connected to optocoupler output (MIDI IN)
#define MIDI_TX_PIN   17   // GPIO connected to MIDI OUT circuit (-1 if unused)
// -----------------------------------------------------------------------

UARTConnection uartMIDI;

// Last processed event index, used to detect new events in the queue.
static int lastEventIndex = -1;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("UART MIDI Basic - starting");

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

        char noteBuf[8];

        if (ev.statusCode == MIDI_NOTE_ON || ev.statusCode == MIDI_NOTE_OFF) {
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
            Serial.print("[MIDI] ");
            Serial.print(ev.statusCode == MIDI_NOTE_ON ? "NoteOn" : "NoteOff");
            Serial.print("  ch=");   Serial.print(ev.channel0 + 1);
            Serial.print("  note="); Serial.print(noteBuf);
            Serial.print("  vel=");  Serial.println(ev.velocity7);
        } else if (ev.statusCode == MIDI_CONTROL_CHANGE) {
            Serial.print("[MIDI] ControlChange");
            Serial.print("  ch=");  Serial.print(ev.channel0 + 1);
            Serial.print("  cc=");  Serial.print(ev.noteNumber);
            Serial.print("  val="); Serial.println(ev.velocity7);
        } else if (ev.statusCode == MIDI_PROGRAM_CHANGE) {
            Serial.print("[MIDI] ProgramChange");
            Serial.print("  ch=");   Serial.print(ev.channel0 + 1);
            Serial.print("  prog="); Serial.println(ev.noteNumber);
        } else if (ev.statusCode == MIDI_PITCH_BEND) {
            Serial.print("[MIDI] PitchBend");
            Serial.print("  ch="); Serial.print(ev.channel0 + 1);
            Serial.print("  pb="); Serial.println(ev.pitchBend14);
        } else {
            Serial.print("[MIDI] ");
            Serial.println(MIDIHandler::statusName(ev.statusCode));
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
