// USB-Hub-Multi-Device -- ESP32_Host_MIDI example
//
// Connect multiple USB MIDI devices through a USB hub.
// MIDI from any device is automatically forwarded to all other devices.
//
// Hardware: ESP32-S3 or ESP32-P4 with USB Host + powered USB hub.
// Note: ESP32-S3 runs at Full-Speed (12 Mbps) -- up to 2-3 devices
//       recommended. ESP32-P4 at High-Speed (480 Mbps) handles more.
//
// Hub sdkconfig: ESP-IDF v5.x supports single-TT hubs natively.
// For multi-TT hubs, ensure CONFIG_USB_HOST_HUB_MULTI_TT=y in sdkconfig.

// Disable built-in single-device USB BEFORE including the library header.
#define ESP32_HOST_MIDI_NO_USB_HOST

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBHubManager.h>

USBHubManager usbHub;

static int lastEventIndex = -1;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("USB Hub Multi-Device MIDI");

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 40;
    midiHandler.begin(cfg);

    if (usbHub.begin(midiHandler)) {
        Serial.println("USB Hub manager started. Connect devices.");
        Serial.println("MIDI routing: all devices forward to each other.");
    } else {
        Serial.println("USB Hub init failed!");
    }
}

void loop() {
    usbHub.task();       // Process pending device add/remove (thread-safe handoff)
    midiHandler.task();  // Poll all transports and dispatch MIDI events

    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;

        char noteBuf[8];
        if (ev.statusCode == MIDI_NOTE_ON) {
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
            Serial.printf("NoteOn  ch=%d note=%s vel=%d\n",
                          ev.channel0 + 1, noteBuf, ev.velocity7);
        } else if (ev.statusCode == MIDI_NOTE_OFF) {
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
            Serial.printf("NoteOff ch=%d note=%s\n",
                          ev.channel0 + 1, noteBuf);
        } else if (ev.statusCode == MIDI_CONTROL_CHANGE) {
            Serial.printf("CC ch=%d cc=%d val=%d\n",
                          ev.channel0 + 1, ev.noteNumber, ev.velocity7);
        }
    }

    // Show connected device count periodically
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        int count = usbHub.connectedDevices();
        Serial.printf("Connected USB MIDI devices: %d\n", count);
    }
}
