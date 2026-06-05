// ESP32_Host_MIDI / T-Display-S3-OSC
// Bidirectional OSC <-> MIDI bridge on the T-Display-S3 (USB host + WiFi/OSC).
//
// Requires: OSC (CNMAT) + LovyanGFX. Set WIFI_SSID/WIFI_PASS/OSC_TARGET_IP in mapping.h.
// Arduino IDE: Board T-Display-S3 (ESP32-S3) | Serial 115200

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include <OSCConnection.h>
#include "mapping.h"
#include "ST7789_Handler.h"

OSCConnection oscMIDI;

static int    lastEventIndex = -1;
static int    inCount        = 0;   // events received from OSC or USB
static int    outCount       = 0;   // events sent as OSC
static bool   wifiReady      = false;
static unsigned long lastWifiCheck = 0;

// ---- helpers -----------------------------------------------------------

// Returns an event-type color for the display.
static uint32_t eventColor(uint8_t statusCode) {
    if (statusCode == MIDI_NOTE_ON)         return OSC_COL_CYAN;
    if (statusCode == MIDI_NOTE_OFF)        return OSC_COL_GRAY;
    if (statusCode == MIDI_CONTROL_CHANGE)  return OSC_COL_YELLOW;
    if (statusCode == MIDI_PITCH_BEND)      return OSC_COL_ORANGE;
    if (statusCode == MIDI_PROGRAM_CHANGE)  return OSC_COL_LIME;
    return OSC_COL_WHITE;
}

// Formats a MIDIEventData into a compact one-line string (max ~27 chars).
static void formatEvent(const MIDIEventData& ev, char* buf, int bufLen) {
    char noteBuf[8];
    if (ev.statusCode == MIDI_NOTE_ON) {
        MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
        snprintf(buf, bufLen, "NOTE+  %-3s  v=%-3d  ch%d",
                 noteBuf, ev.velocity7, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_NOTE_OFF) {
        MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf));
        snprintf(buf, bufLen, "NOTE-  %-3s  v=%-3d  ch%d",
                 noteBuf, ev.velocity7, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_CONTROL_CHANGE) {
        snprintf(buf, bufLen, "CC#%-3d  v=%-3d       ch%d",
                 ev.noteNumber, ev.velocity7, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_PITCH_BEND) {
        int bend = ev.pitchBend14 - 8192;  // center to ±8192
        snprintf(buf, bufLen, "PB  %+6d            ch%d", bend, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_PROGRAM_CHANGE) {
        snprintf(buf, bufLen, "PC   prog=%-3d         ch%d",
                 ev.noteNumber, ev.channel0 + 1);
    } else if (ev.statusCode == MIDI_CHANNEL_PRESSURE) {
        snprintf(buf, bufLen, "AT   pres=%-3d         ch%d",
                 ev.velocity7, ev.channel0 + 1);
    } else {
        snprintf(buf, bufLen, "%-8s                 ch%d",
                 MIDIHandler::statusName(ev.statusCode), ev.channel0 + 1);
    }
}

// Reconstructs raw MIDI bytes from a MIDIEventData and sends them as OSC.
static bool forwardToOSC(const MIDIEventData& ev) {
    uint8_t data[3];
    int     len = 0;

    if (ev.statusCode == MIDI_NOTE_ON) {
        data[0] = 0x90 | (ev.channel0 & 0x0F);
        data[1] = (uint8_t)ev.noteNumber;
        data[2] = (uint8_t)ev.velocity7;
        len = 3;
    } else if (ev.statusCode == MIDI_NOTE_OFF) {
        data[0] = 0x80 | (ev.channel0 & 0x0F);
        data[1] = (uint8_t)ev.noteNumber;
        data[2] = (uint8_t)ev.velocity7;
        len = 3;
    } else if (ev.statusCode == MIDI_CONTROL_CHANGE) {
        data[0] = 0xB0 | (ev.channel0 & 0x0F);
        data[1] = (uint8_t)ev.noteNumber;
        data[2] = (uint8_t)ev.velocity7;
        len = 3;
    } else if (ev.statusCode == MIDI_PROGRAM_CHANGE) {
        data[0] = 0xC0 | (ev.channel0 & 0x0F);
        data[1] = (uint8_t)ev.noteNumber;
        len = 2;
    } else if (ev.statusCode == MIDI_CHANNEL_PRESSURE) {
        data[0] = 0xD0 | (ev.channel0 & 0x0F);
        data[1] = (uint8_t)ev.velocity7;
        len = 2;
    } else if (ev.statusCode == MIDI_PITCH_BEND) {
        int     pb  = ev.pitchBend14;               // 0-16383
        data[0] = 0xE0 | (ev.channel0 & 0x0F);
        data[1] = (uint8_t)(pb & 0x7F);
        data[2] = (uint8_t)((pb >> 7) & 0x7F);
        len = 3;
    }

    if (len > 0) return oscMIDI.sendMidiMessage(data, len);
    return false;
}

// ---- setup / loop ------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(200);

    // Power on (needed when running from battery on T-Display-S3)
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    display.init();

    // Connect WiFi
    Serial.print("Connecting to " WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(400);
        Serial.print(".");
    }
    Serial.println();
    wifiReady = true;
    Serial.println("WiFi: " + WiFi.localIP().toString());

    if (!oscMIDI.begin(OSC_LOCAL_PORT, OSC_TARGET_IP, OSC_REMOTE_PORT)) {
        Serial.println("Failed to open UDP socket.");
        while (true) delay(1000);
    }

    // Update display with real connection info
    display.setWifi(true, WiFi.localIP().toString().c_str(),
                    OSC_LOCAL_PORT, OSC_REMOTE_PORT);

    midiHandler.addTransport(&oscMIDI);
    MIDIHandlerConfig cfg;
    cfg.maxEvents = 40;
    midiHandler.begin(cfg);

    Serial.println("Ready.");
}

void loop() {
    midiHandler.task();

    // Refresh WiFi dot every 5 s
    if (millis() - lastWifiCheck >= 5000) {
        lastWifiCheck = millis();
        bool connected = (WiFi.status() == WL_CONNECTED);
        if (connected != wifiReady) {
            wifiReady = connected;
            display.setWifi(connected, WiFi.localIP().toString().c_str(),
                            OSC_LOCAL_PORT, OSC_REMOTE_PORT);
        }
    }

    // Process new MIDI events
    const auto& queue = midiHandler.getQueue();
    bool countersChanged = false;

    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;
        inCount++;
        countersChanged = true;

        // Push to display
        char line[32];
        formatEvent(ev, line, sizeof(line));
        display.pushEvent(eventColor(ev.statusCode), line);

        // Forward to OSC target
        if (forwardToOSC(ev)) {
            outCount++;
        }

        // Serial log
        Serial.printf("[MIDI] %-12s ch=%-2d  %s\n",
                      MIDIHandler::statusName(ev.statusCode), ev.channel0 + 1, line);
    }

    if (countersChanged) {
        display.setCounters(inCount, outCount);
    }
}
