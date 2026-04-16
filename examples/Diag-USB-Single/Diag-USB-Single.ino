// Diag-USB-Single -- Minimal single-device USB MIDI Host (no BLE, no hub)
//
// Tests whether USBConnection can claim the MIDI interface.
// Compare with Diag-USB-Hub to isolate hub-manager issues.

#define ESP32_HOST_MIDI_NO_BLE

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Diag-USB-Single: minimal USB MIDI Host (no BLE, no hub)");

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 10;
    midiHandler.begin(cfg);

    Serial.println("Ready. Plug a USB MIDI device.");
}

void loop() {
    midiHandler.task();

    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        static int last = -1;
        if (ev.index <= last) continue;
        last = ev.index;
        if (ev.statusCode == MIDI_NOTE_ON) {
            Serial.printf("NoteOn ch=%d note=%d vel=%d\n",
                          ev.channel0 + 1, ev.noteNumber, ev.velocity7);
        }
    }

    static unsigned long t = 0;
    if (millis() - t > 5000) {
        t = millis();
        Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    }
}
