// T-Display-S3-OSC — ESP32_Host_MIDI example
//
// Bidirectional OSC ↔ MIDI bridge on the T-Display-S3 (LilyGO):
//   USB keyboard  →  ESP32  →  OSC/UDP  →  Max/MSP, Pure Data, SuperCollider
//   Max/MSP, PD   →  OSC/UDP  →  ESP32  →  MIDI (displayed + forwarded)
//
// The built-in 1.9" display shows:
//   - WiFi connection status and IP address
//   - Local / remote OSC ports
//   - Live scrolling MIDI event log (color-coded by message type)
//   - IN / OUT event counters
//
// Requirements:
//   1. Install "OSC" library by Adrian Freed / CNMAT via Arduino Library Manager.
//   2. Fill in WIFI_SSID, WIFI_PASS and OSC_TARGET_IP in mapping.h.
//   3. In Max/MSP: [udpreceive 8000] → [OSC-route /midi/noteon] → …
//
// OSC address map:
//   /midi/noteon      channel note velocity
//   /midi/noteoff     channel note velocity
//   /midi/cc          channel controller value
//   /midi/pc          channel program
//   /midi/pitchbend   channel bend  (-8192 … 8191)
//   /midi/aftertouch  channel pressure

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "../../src/OSCConnection.h"
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
static uint32_t eventColor(const std::string& status) {
    if (status == "NoteOn")         return OSC_COL_CYAN;
    if (status == "NoteOff")        return OSC_COL_GRAY;
    if (status == "ControlChange")  return OSC_COL_YELLOW;
    if (status == "PitchBend")      return OSC_COL_ORANGE;
    if (status == "ProgramChange")  return OSC_COL_LIME;
    return OSC_COL_WHITE;
}

// Formats a MIDIEventData into a compact one-line string (max ~27 chars).
static void formatEvent(const MIDIEventData& ev, char* buf, int bufLen) {
    if (ev.status == "NoteOn") {
        snprintf(buf, bufLen, "NOTE+  %-3s  v=%-3d  ch%d",
                 ev.noteOctave.c_str(), ev.velocity, ev.channel);
    } else if (ev.status == "NoteOff") {
        snprintf(buf, bufLen, "NOTE-  %-3s  v=%-3d  ch%d",
                 ev.noteOctave.c_str(), ev.velocity, ev.channel);
    } else if (ev.status == "ControlChange") {
        snprintf(buf, bufLen, "CC#%-3d  v=%-3d       ch%d",
                 ev.note, ev.velocity, ev.channel);
    } else if (ev.status == "PitchBend") {
        int bend = ev.pitchBend - 8192;  // center to ±8192
        snprintf(buf, bufLen, "PB  %+6d            ch%d", bend, ev.channel);
    } else if (ev.status == "ProgramChange") {
        snprintf(buf, bufLen, "PC   prog=%-3d         ch%d",
                 ev.note, ev.channel);
    } else if (ev.status == "AfterTouch") {
        snprintf(buf, bufLen, "AT   pres=%-3d         ch%d",
                 ev.velocity, ev.channel);
    } else {
        snprintf(buf, bufLen, "%-8s                 ch%d",
                 ev.status.c_str(), ev.channel);
    }
}

// Reconstructs raw MIDI bytes from a MIDIEventData and sends them as OSC.
static bool forwardToOSC(const MIDIEventData& ev) {
    uint8_t data[3];
    int     len = 0;

    if (ev.status == "NoteOn") {
        data[0] = 0x90 | ((ev.channel - 1) & 0x0F);
        data[1] = (uint8_t)ev.note;
        data[2] = (uint8_t)ev.velocity;
        len = 3;
    } else if (ev.status == "NoteOff") {
        data[0] = 0x80 | ((ev.channel - 1) & 0x0F);
        data[1] = (uint8_t)ev.note;
        data[2] = (uint8_t)ev.velocity;
        len = 3;
    } else if (ev.status == "ControlChange") {
        data[0] = 0xB0 | ((ev.channel - 1) & 0x0F);
        data[1] = (uint8_t)ev.note;
        data[2] = (uint8_t)ev.velocity;
        len = 3;
    } else if (ev.status == "ProgramChange") {
        data[0] = 0xC0 | ((ev.channel - 1) & 0x0F);
        data[1] = (uint8_t)ev.note;
        len = 2;
    } else if (ev.status == "AfterTouch") {
        data[0] = 0xD0 | ((ev.channel - 1) & 0x0F);
        data[1] = (uint8_t)ev.velocity;
        len = 2;
    } else if (ev.status == "PitchBend") {
        int     pb  = ev.pitchBend;                 // 0-16383
        data[0] = 0xE0 | ((ev.channel - 1) & 0x0F);
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
        display.pushEvent(eventColor(ev.status), line);

        // Forward to OSC target
        if (forwardToOSC(ev)) {
            outCount++;
        }

        // Serial log
        Serial.printf("[MIDI] %-12s ch=%-2d  %s\n",
                      ev.status.c_str(), ev.channel, line);
    }

    if (countersChanged) {
        display.setCounters(inCount, outCount);
    }
}
