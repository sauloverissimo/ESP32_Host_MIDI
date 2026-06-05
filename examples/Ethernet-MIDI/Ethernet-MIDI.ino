// ESP32_Host_MIDI / Ethernet-MIDI
// RTP-MIDI (AppleMIDI) over wired Ethernet (W5x00 SPI, or ESP32-P4 native MAC).
//
// macOS/iOS discover it as a standard network MIDI port (enter the device IP
// manually). On ESP32-S3 it runs USB Host at the same time, forwarding a USB
// keyboard to the DAW over Ethernet. Wiring and setup are in the README.
//
// Requires: AppleMIDI (lathoub, v3.x) + Ethernet libraries.
// Arduino IDE: Board ESP32-S3 (USB host) or ESP32-P4 | Serial 115200

#include <Arduino.h>
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include <EthernetMIDIConnection.h>
#include "mapping.h"

// ---- RTP-MIDI device name (shown in macOS/iOS Audio MIDI Setup) --------
#define DEVICE_NAME  "ESP32 MIDI"
// -----------------------------------------------------------------------

EthernetMIDIConnection ethMIDI;

static int lastEventIndex = -1;
static unsigned long lastStatusPrint = 0;

// -----------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Ethernet MIDI - starting");

    // Optional: configure non-default SPI pins before begin().
    // SPI.begin(ETH_SCK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN, ETH_CS_PIN);

#if USE_DHCP
    Serial.print("Requesting DHCP address");
    bool ok = ethMIDI.begin(MY_MAC, IPAddress(0, 0, 0, 0), ETH_CS_PIN);
#else
    Serial.print("Using static IP " + STATIC_IP.toString());
    bool ok = ethMIDI.begin(MY_MAC, STATIC_IP, ETH_CS_PIN);
#endif

    if (!ok) {
        Serial.println(" - FAILED. Check W5500 wiring and cable.");
        while (true) delay(1000);
    }

    Serial.println();
    Serial.println("  IP  : " + ethMIDI.localIP().toString());
    Serial.println("  Port: " + String(ETH_MIDI_PORT));
    Serial.println("  Audio MIDI Setup > Network > Add session > enter this IP.");

    midiHandler.addTransport(&ethMIDI);

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 30;
    midiHandler.begin(cfg);

    Serial.println("Ready. Waiting for macOS/iOS to connect...");
}

void loop() {
    midiHandler.task();

    // Print connection status every 5 seconds.
    if (millis() - lastStatusPrint >= 5000) {
        lastStatusPrint = millis();
        if (ethMIDI.isConnected()) {
            Serial.println("[ETH] " + String(ethMIDI.connectedCount()) + " peer(s) connected");
        } else {
            Serial.println("[ETH] Waiting for connection...");
        }
    }

    // Print all new MIDI events.
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
}
