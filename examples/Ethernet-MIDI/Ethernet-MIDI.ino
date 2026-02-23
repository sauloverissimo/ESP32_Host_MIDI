// Ethernet-MIDI — ESP32_Host_MIDI example
//
// Exposes the ESP32 as an RTP-MIDI (AppleMIDI) device over wired Ethernet
// using a W5x00 SPI module (W5500 recommended).
// macOS and iOS discover it as a standard MIDI port — no driver, no USB cable.
//
// Compared to WiFi RTP-MIDI:
//   - Lower and more consistent latency (no WiFi jitter)
//   - No mDNS auto-discovery: enter the device IP manually in Audio MIDI Setup
//   - Ideal for studio racks, ESP32-P4 (native Ethernet MAC), or where WiFi
//     is unreliable
//
// Requirements:
//   1. Install "AppleMIDI" library by lathoub (v3.x) via Arduino Library Manager.
//   2. Install "Ethernet" library (built-in) or "Ethernet_Generic" for W5500.
//   3. Connect W5500 module via SPI — see mapping.h for pin assignments.
//   4. Adjust MAC address and IP settings in mapping.h.
//   5. On macOS: Audio MIDI Setup → Network → click "+" → enter device IP
//      and port 5004 → Connect.
//
// On ESP32-S3 this also enables USB Host simultaneously: play a USB keyboard
// and notes are forwarded over Ethernet to your DAW in real time.

#include <Arduino.h>
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "../../src/EthernetMIDIConnection.h"  // or "EthernetMIDIConnection.h" when library is installed
                                                // Requires: AppleMIDI + Ethernet libraries
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
    Serial.println("Ethernet MIDI — starting");

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
        Serial.println(" — FAILED. Check W5500 wiring and cable.");
        while (true) delay(1000);
    }

    Serial.println();
    Serial.println("  IP  : " + ethMIDI.localIP().toString());
    Serial.println("  Port: " + String(ETH_MIDI_PORT));
    Serial.println("  → Audio MIDI Setup → Network → Add session → enter this IP.");

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
}
