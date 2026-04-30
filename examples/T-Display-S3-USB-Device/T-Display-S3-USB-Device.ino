// T-Display-S3-USB-Device — ESP32_Host_MIDI example
//
// The ESP32-S3 acts as a USB MIDI interface for the connected computer,
// while simultaneously receiving MIDI from a BLE device (iPhone, iPad,
// BLE keyboard, etc.).
//
// Signal path:
//   iPhone / iPad  ──BLE──►  ESP32-S3  ──USB──►  Logic / Ableton / Reaper
//
// The 1.9" display shows:
//   - USB connection status (waiting / connected)
//   - BLE connection status (scanning / connected)
//   - Scrolling MIDI event log (color-coded by message type)
//   - IN / OUT event counters
//
// Arduino IDE setup (mandatory):
//   Tools > Board    → T-Display-S3 (or ESP32S3 Dev Module)
//   Tools > USB Mode → USB-OTG (TinyUSB)   ← required for USB MIDI device
//
// The device name shown in the DAW's MIDI port list is configured in
// mapping.h via USB_MIDI_DEVICE_NAME.

#include <Arduino.h>
#define ESP32_HOST_MIDI_NO_USB_HOST  // Use USB Device mode, not Host
#include <ESP32_Host_MIDI.h>
#include <BLEConnection.h>           // v6.0: transports are no longer auto-included
#include "../../src/USBDeviceConnection.h"
#include "mapping.h"
#include "ST7789_Handler.h"

// MUST be global. TinyUSB registers the USB descriptor on construction,
// before USB.begin() is called. The name comes from mapping.h.
USBDeviceConnection usbMIDI(USB_MIDI_DEVICE_NAME);

// v6.0: BLE transport is also explicit. v5 auto-started BLE inside
// midiHandler.begin(); v6 leaves the user in charge.
BLEConnection bleHost;

static int    lastEventIndex = -1;
static int    inCount        = 0;
static int    outCount       = 0;
static bool   lastBLE        = false;
static unsigned long lastStatusCheck = 0;

// ---- helpers -----------------------------------------------------------

static uint32_t eventColor(uint8_t code) {
    if (code == MIDI_NOTE_ON)        return USB_COL_CYAN;
    if (code == MIDI_NOTE_OFF)       return USB_COL_GRAY;
    if (code == MIDI_CONTROL_CHANGE) return USB_COL_YELLOW;
    if (code == MIDI_PITCH_BEND)     return USB_COL_MAGENTA;
    if (code == MIDI_PROGRAM_CHANGE) return USB_COL_LIME;
    return USB_COL_WHITE;
}

static void formatEvent(const MIDIEventData& ev, char* buf, int len) {
    char noteBuf[8];
    if (ev.statusCode == MIDI_NOTE_ON) {
        MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
        snprintf(buf, len, "NOTE+  %-3s  v=%-3d  ch%d",
                 noteBuf, ev.velocity7, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_NOTE_OFF) {
        MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
        snprintf(buf, len, "NOTE-  %-3s  v=%-3d  ch%d",
                 noteBuf, ev.velocity7, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_CONTROL_CHANGE) {
        snprintf(buf, len, "CC#%-3d  v=%-3d        ch%d",
                 ev.noteNumber, ev.velocity7, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_PITCH_BEND) {
        snprintf(buf, len, "PB  %+6d             ch%d",
                 (int)ev.pitchBend14 - 8192, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_PROGRAM_CHANGE) {
        snprintf(buf, len, "PC   prog=%-3d          ch%d",
                 ev.noteNumber, ev.channel0 + 1);
    } else {
        snprintf(buf, len, "%-8s                ch%d",
                 MIDIHandler::statusName(ev.statusCode), ev.channel0 + 1);
    }
}

// ---- setup / loop ------------------------------------------------------

void setup() {
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    display.init();

    // v6.0: register both transports explicitly. USB Device first,
    // then BLE peripheral. Each transport owns its own begin().
    midiHandler.addTransport(&usbMIDI);
    midiHandler.addTransport(&bleHost);

    // Start USB stack (enumerates on the host as a MIDI device).
    usbMIDI.begin();

    // Start BLE peripheral, advertising the same name as USB Device.
    bleHost.begin(USB_MIDI_DEVICE_NAME);

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 40;
    midiHandler.begin(cfg);

    display.setBLE(false);
    display.setUSB(false);
}

void loop() {
    midiHandler.task();

    // Refresh connection dots every 2 s.
    if (millis() - lastStatusCheck >= 2000) {
        lastStatusCheck = millis();

        // v6.0: query BLE state directly on the BLEConnection instance.
        // MIDIHandler::isBleConnected was removed when the built-in BLE
        // member was dropped.
        bool ble = bleHost.isConnected();
        if (ble != lastBLE) {
            lastBLE = ble;
            display.setBLE(ble);
        }

        // USB MIDI device has no per-session state — show "connected" once
        // begin() has been called (the port is always visible to the host).
        display.setUSB(usbMIDI.isConnected());
    }

    // Process new MIDI events.
    const auto& queue = midiHandler.getQueue();
    bool countersChanged = false;

    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;
        inCount++;
        countersChanged = true;

        char line[32];
        formatEvent(ev, line, sizeof(line));
        display.pushEvent(eventColor(ev.statusCode), line);

        // Forward every event received (from BLE or USB host) to the other side.
        // BLE → forward to USB; USB → forward to BLE (if connected).
        // midiHandler forwards automatically to all registered transports.
        outCount++;
    }

    if (countersChanged) {
        display.setCounters(inCount, outCount);
    }
}
