// Diag-USB-Hub -- Minimal hub-mode USB MIDI Host (no BLE)
//
// Tests whether USBHubManager can claim the MIDI interface.
// Compare with Diag-USB-Single to isolate hub-manager issues.

#define ESP32_HOST_MIDI_NO_BLE
#define ESP32_HOST_MIDI_NO_USB_HOST

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBHubManager.h>

USBHubManager usbHub;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Diag-USB-Hub: minimal hub-mode USB MIDI Host (no BLE)");

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 10;
    midiHandler.begin(cfg);

    if (usbHub.begin(midiHandler)) {
        Serial.println("Hub manager started. Plug a USB MIDI device.");
    } else {
        Serial.println("Hub manager init FAILED!");
    }
}

void loop() {
    usbHub.task();
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
        Serial.printf("Connected: %d | Free heap: %d\n",
                      usbHub.connectedDevices(), ESP.getFreeHeap());
    }
}
