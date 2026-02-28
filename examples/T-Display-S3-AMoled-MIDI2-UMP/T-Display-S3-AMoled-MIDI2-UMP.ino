// T-Display-S3-AMoled-MIDI2-UMP — ESP32_Host_MIDI example
//
// Two T-Display-S3 AMOLED boards exchange MIDI 2.0 (Universal MIDI Packet)
// over WiFi UDP.  The 1.91" RM67162 AMOLED shows (landscape 536×240):
//
//   ■ (RX dot)   — pulses cyan on every received packet
//   WiFi | Peer  — real IPs, green when active
//   LAST NOTE    — large note name, 16-bit velocity bar, exact value
//   EVENTS       — scrolling 6-line log with 32-bit MIDI 2.0 values
//   IN / OUT / VEL% — packet counters + velocity preset
//
// Button (GPIO 0 / BOOT only — AMOLED board has no GPIO 14 button):
//   Short press  — send a test NoteOn immediately
//   Long press   — cycle demo velocity preset (25 / 50 / 75 / 100 %)
//
// Quick-start:
//   1. Set WIFI_SSID / WIFI_PASS in mapping.h.
//   2. Flash to BOTH boards.  Read each board's "My IP:" from Serial.
//   3. Set PEER_IP on each board to the OTHER board's IP.  Re-flash.
//   4. Short-press BOOT on either board — the other board's display reacts.
//
// Note: AMOLED displays can suffer from burn-in with static content.
//       Avoid leaving the display on at maximum brightness for long periods.
//
// Arduino IDE:
//   Tools > Board            → ESP32S3 Dev Module (or T-Display-S3 AMOLED)
//   Tools > Partition Scheme → Huge APP (3MB No OTA)

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "../../src/MIDI2Support.h"
#include "../../src/MIDI2UDPConnection.h"
#include "mapping.h"
#include "RM67162_Handler.h"

MIDI2UDPConnection midi2udp;

// ── State ────────────────────────────────────────────────────────────────────
static int           lastEventIndex = -1;
static int           inCount        = 0;
static int           outCount       = 0;
static unsigned long lastRxMs       = 0;
static unsigned long dotOffMs       = 0;
static bool          dotOn          = false;
static bool          peerWasActive  = false;
static unsigned long lastDemoMs     = 0;
static uint8_t       demoNoteIdx    = 0;

// Demo velocity presets: 25 / 50 / 75 / 100 %  (16-bit: 0x3FFF / 0x7FFF / 0xBFFF / 0xFFFF)
static const uint8_t VEL_PRESETS[] = { 32, 64, 96, 127 };  // 7-bit → scales to 16-bit
static uint8_t       velPresetIdx  = 3;  // start at 100%

// Demo notes (C-major)
static const uint8_t DEMO_NOTES[] = { 60, 62, 64, 65, 67, 69, 71, 72 };

// ── Single-button state machine (GPIO 0 / BOOT) ─────────────────────────────
// Short press: send test note  |  Long press: cycle velocity preset
static bool          btn0Prev      = HIGH;
static unsigned long btn0PressedAt = 0;
static bool          btn0Handled   = false;  // true after long-press fires

static void handleButton() {
    bool cur = digitalRead(0);
    unsigned long now = millis();

    // Falling edge — button just pressed
    if (cur == LOW && btn0Prev == HIGH) {
        btn0PressedAt = now;
        btn0Handled   = false;
    }

    // Button still held — check for long press
    if (cur == LOW && !btn0Handled &&
        (now - btn0PressedAt >= BTN_LONG_PRESS_MS))
    {
        btn0Handled = true;  // fire once while held
        // ── LONG PRESS: cycle velocity preset ──
        velPresetIdx = (velPresetIdx + 1) % 4;
        uint8_t pct = (uint8_t)((uint32_t)VEL_PRESETS[velPresetIdx] * 100 / 127);
        Serial.printf("[LONG] velocity preset -> %d%%\n", pct);
        uint16_t vel16 = MIDI2Scaler::scale7to16(VEL_PRESETS[velPresetIdx]);
        display.setLastNote("VEL", vel16);
        display.setCounters(inCount, outCount, pct);
    }

    // Rising edge — button released
    if (cur == HIGH && btn0Prev == LOW) {
        if (!btn0Handled && (now - btn0PressedAt >= BTN_DEBOUNCE_MS)) {
            // ── SHORT PRESS: send test note ──
            Serial.printf("[SHORT] sending test note vel=%d\n", VEL_PRESETS[velPresetIdx]);
            sendTestNote();
        }
        btn0Handled = false;
    }

    btn0Prev = cur;
}

// ── Format helpers ────────────────────────────────────────────────────────────
static uint32_t eventColor(const MIDIEventData& ev) {
    if (ev.status == "NoteOn")        return M2_COL_CYAN;
    if (ev.status == "NoteOff")       return M2_COL_GRAY;
    if (ev.status == "ControlChange") return M2_COL_YELLOW;
    if (ev.status == "PitchBend")     return M2_COL_MAGENTA;
    if (ev.status == "ProgramChange") return M2_COL_LIME;
    return M2_COL_WHITE;
}

// Landscape 536px wide — font-1 chars are 6px, margin 4px → ~88 chars/line
static void formatEvent(const MIDIEventData& ev, const UMPResult& r,
                        char* buf, int len) {
    if (ev.status == "NoteOn" && r.valid && r.isMIDI2) {
        uint16_t v16 = (uint16_t)(r.value >> 16);
        uint8_t  pct = (uint8_t)((uint32_t)v16 * 100 / 65535);
        snprintf(buf, len, "+%-3s  %3d%%  vel=%-5u  (16-bit)",
                 ev.noteOctave.c_str(), pct, v16);
    } else if (ev.status == "NoteOff") {
        snprintf(buf, len, "-%-3s", ev.noteOctave.c_str());
    } else if (ev.status == "ControlChange" && r.valid && r.isMIDI2) {
        snprintf(buf, len, "CC%-3d  val=%-10u  (32-bit)", ev.note, r.value);
    } else if (ev.status == "PitchBend" && r.valid && r.isMIDI2) {
        int32_t pb = (int32_t)(r.value - 0x80000000UL);
        snprintf(buf, len, "PitchBend  %+11ld  (32-bit)", (long)pb);
    } else if (ev.status == "ProgramChange") {
        snprintf(buf, len, "PC  prog=%-3d  ch%d", ev.velocity, ev.channel);
    } else {
        snprintf(buf, len, "%-12s  ch%d", ev.status.c_str(), ev.channel);
    }
}

// ── Helper: send a test note with current velocity preset ────────────────────
static void sendTestNote() {
    if ((uint32_t)(IPAddress)PEER_IP == 0) return;  // no peer configured
    uint8_t note = DEMO_NOTES[demoNoteIdx % sizeof(DEMO_NOTES)];
    demoNoteIdx  = (demoNoteIdx + 1) % sizeof(DEMO_NOTES);
    uint8_t vel  = VEL_PRESETS[velPresetIdx];
    midiHandler.sendNoteOn(1, note, vel);
    outCount++;
    delay(120);
    midiHandler.sendNoteOff(1, note, 0);
}

// ── setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);

    // Button — only GPIO 0 (BOOT) available on T-Display-S3 AMOLED
    pinMode(0, INPUT_PULLUP);

    // Display init handles power-on (GPIO 9) internally
    display.init();

    // WiFi
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

    String myIP = WiFi.localIP().toString();
    Serial.printf("\nMy IP : %s\n", myIP.c_str());
    display.setWiFi(true, myIP.c_str());

    // Show peer target
    IPAddress peerIP = PEER_IP;
    if ((uint32_t)peerIP != 0) {
        Serial.printf("Target: %s:%d\n", peerIP.toString().c_str(), MIDI2_UDP_REMOTE_PORT);
        char peerStr[20];
        peerIP.toString().toCharArray(peerStr, sizeof(peerStr));
        display.setPeer(false, peerStr);  // orange until first packet received
    } else {
        Serial.println("Target: (none — set PEER_IP in mapping.h)");
        display.setPeer(false);
    }

    // MIDI 2.0 UDP transport
    midi2udp.begin(MIDI2_UDP_LOCAL_PORT, PEER_IP, MIDI2_UDP_REMOTE_PORT);
    midiHandler.addTransport(&midi2udp);

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 32;
    midiHandler.begin(cfg);

    uint8_t velPct = (uint8_t)((uint32_t)VEL_PRESETS[velPresetIdx] * 100 / 127);
    display.setCounters(0, 0, velPct);

    Serial.printf("Ready. Short press=send note, Long press=cycle velocity (%d%%)\n",
                  velPct);
}

// ── loop ─────────────────────────────────────────────────────────────────────
void loop() {
    midiHandler.task();

    // ── Button (single GPIO 0: short=send note, long=cycle velocity) ────────
    handleButton();

    // ── RX dot off after 120 ms ──────────────────────────────────────────────
    if (dotOn && millis() - dotOffMs >= 120) {
        dotOn = false;
        display.pulseRxDot(false);
    }

    // ── Process queue ────────────────────────────────────────────────────────
    const auto& queue = midiHandler.getQueue();
    bool countersChanged = false;

    for (const auto& ev : queue) {
        if (ev.index <= lastEventIndex) continue;
        lastEventIndex = ev.index;
        inCount++;
        countersChanged = true;
        lastRxMs = millis();

        UMPResult r = midi2udp.lastResult();

        // Flash RX dot
        dotOffMs = millis();
        if (!dotOn) { dotOn = true; display.pulseRxDot(true); }

        // Update LAST NOTE panel for note events
        if (ev.status == "NoteOn" && r.valid && r.isMIDI2) {
            uint16_t vel16 = (uint16_t)(r.value >> 16);
            display.setLastNote(ev.noteOctave.c_str(), vel16);
        }

        // Push to event log
        char line[90];
        formatEvent(ev, r, line, sizeof(line));
        display.pushEvent(eventColor(ev), line);

        // Serial mirror
        Serial.printf("[MIDI 1.0] %-12s ch%d note=%d vel=%d\n",
                      ev.status.c_str(), ev.channel, ev.note, ev.velocity);
        if (r.valid && r.isMIDI2)
            Serial.printf("[MIDI 2.0] opcode=0x%X  val=%u  (32-bit)\n\n",
                          r.opcode, r.value);
    }

    // ── Peer status — active when receiving packets, independent of PEER_IP ──
    bool peerActive = (lastRxMs > 0) && (millis() - lastRxMs < 8000);
    if (peerActive != peerWasActive) {
        peerWasActive = peerActive;
        IPAddress p = PEER_IP;
        char peerStr[20] = "";
        if ((uint32_t)p != 0)
            p.toString().toCharArray(peerStr, sizeof(peerStr));
        display.setPeer(peerActive, peerStr);
    }

    // ── Counters — update on either IN or OUT change ─────────────────────────
    static int lastOutCount = -1;
    if (countersChanged || outCount != lastOutCount) {
        lastOutCount = outCount;
        uint8_t velPct = (uint8_t)((uint32_t)VEL_PRESETS[velPresetIdx] * 100 / 127);
        display.setCounters(inCount, outCount, velPct);
    }

    // ── Demo loop ─────────────────────────────────────────────────────────────
#if DEMO_LOOP_MS > 0
    if ((uint32_t)(IPAddress)PEER_IP != 0 &&
        millis() - lastDemoMs >= (unsigned long)DEMO_LOOP_MS)
    {
        lastDemoMs = millis();
        sendTestNote();
    }
#endif
}
