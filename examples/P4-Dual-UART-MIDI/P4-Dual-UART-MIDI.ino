// P4-Dual-UART-MIDI — ESP32_Host_MIDI example
//
// Demonstrates two simultaneous MIDI DIN-5 ports on the ESP32-P4,
// which provides 5 hardware UARTs. Both ports share the same MIDIHandler
// and produce events in the same queue.
//
// ESP32-P4 highlights relevant to this sketch:
//   - USB 2.0 High-Speed Host (480 Mbps) — add a USB teclado in parallel if needed
//   - 5× UART → MIDI IN 1 + MIDI IN 2 + MIDI OUT 1 + MIDI OUT 2 (+ Serial debug)
//   - Native Ethernet MAC (ESP32_HOST_MIDI_HAS_ETH_MAC=1) — ready for RTP-MIDI
//   - No native WiFi/BLE: use an ESP32-C6 module for wireless if needed
//
// Wiring (MIDI IN — requires optocoupler per port, e.g. 6N138 or TLP2361):
//   Port 1: DIN-5 pin 5 ──── 220Ω ──── optocoupler ──── MIDI1_RX_PIN
//   Port 2: DIN-5 pin 5 ──── 220Ω ──── optocoupler ──── MIDI2_RX_PIN
//
// Wiring (MIDI OUT — two 220Ω resistors per port, no optocoupler on TX):
//   Port 1: MIDI1_TX_PIN ──── 220Ω ──── DIN-5 pin 5
//   Port 2: MIDI2_TX_PIN ──── 220Ω ──── DIN-5 pin 5
//
// Adjust pin defines below for your PCB.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include "../../src/UARTConnection.h"  // or "UARTConnection.h" when library is installed

#ifndef CONFIG_IDF_TARGET_ESP32P4
  #warning "This example is optimised for ESP32-P4. It will still compile on other targets."
#endif

// ---- Pin configuration -----------------------------------------------
// Port 1 — e.g. UART1
#define MIDI1_RX_PIN   18
#define MIDI1_TX_PIN   17

// Port 2 — e.g. UART2
#define MIDI2_RX_PIN   16
#define MIDI2_TX_PIN   15
// ----------------------------------------------------------------------

UARTConnection midiPort1;
UARTConnection midiPort2;

static int lastEventIndex = -1;

// Prints one MIDI event to the debug serial.
static void printEvent(const MIDIEventData& ev) {
    // Tag the source port (index embedded via chordIndex or by transport — here we
    // use the raw queue order and just label for clarity).
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

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("P4 Dual UART MIDI — starting");

#ifdef ESP32_HOST_MIDI_HAS_ETH_MAC
    Serial.println("  [P4] Native Ethernet MAC detected (ESP32_HOST_MIDI_HAS_ETH_MAC=1)");
    Serial.println("       Add a LAN8720 PHY + RTP-MIDI for wired DAW connection.");
#endif

    // ---- MIDI Port 1 (Serial1) ----
    if (midiPort1.begin(Serial1, MIDI1_RX_PIN, MIDI1_TX_PIN)) {
        Serial.println("  Port 1 open — RX:" + String(MIDI1_RX_PIN) +
                       " TX:" + String(MIDI1_TX_PIN));
    }

    // ---- MIDI Port 2 (Serial2) ----
    if (midiPort2.begin(Serial2, MIDI2_RX_PIN, MIDI2_TX_PIN)) {
        Serial.println("  Port 2 open — RX:" + String(MIDI2_RX_PIN) +
                       " TX:" + String(MIDI2_TX_PIN));
    }

    // Register both as external transports — same handler, single queue.
    midiHandler.addTransport(&midiPort1);
    midiHandler.addTransport(&midiPort2);

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 40;  // More room for two simultaneous ports
    midiHandler.begin(cfg);

    Serial.println("Ready. Connect MIDI devices to ports 1 and 2.");
}

void loop() {
    midiHandler.task();

    // Process all new events from both ports.
    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;
        printEvent(ev);
    }

    // Demo: relay NoteOn events received on port 1 out through port 2.
    // Remove or adapt for your application.
    // (This runs from the queue, so it's safe — no double-dispatch.)
}
