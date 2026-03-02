#ifndef MAPPING_H
#define MAPPING_H

// ── T-Display-S3 AMOLED 1.91" hardware ─────────────────────────────────────
// RM67162 AMOLED — no backlight pin (self-emitting, brightness via panel cmd)
// Power enable on GPIO 9 (HIGH = on)
#define PIN_POWER_ON   9

// ── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID  "Floresta"
#define WIFI_PASS  "q1w2e3r4"

// ── MIDI 2.0 UDP ports ─────────────────────────────────────────────────────
#define MIDI2_UDP_LOCAL_PORT   5006
#define MIDI2_UDP_REMOTE_PORT  5006

// ── Peer IP ─────────────────────────────────────────────────────────────────
// After flashing BOTH boards, open Serial (115200) and note each board's
// "My IP:" line.  Then set each board's PEER_IP to the OTHER board's address
// and re-flash.  Example:
//   Board A  →  PEER_IP = IPAddress(192,168,1,43)  (Board B's IP)
//   Board B  →  PEER_IP = IPAddress(192,168,1,42)  (Board A's IP)
#define PEER_IP  IPAddress(0, 0, 0, 0)

// ── Demo loop ───────────────────────────────────────────────────────────────
// Sends a note every N ms so you can test without a physical MIDI source.
// Set to 0 to disable.
#define DEMO_LOOP_MS  0

// ── Button timing (single button: GPIO 0 / BOOT) ───────────────────────────
#define BTN_DEBOUNCE_MS     50
#define BTN_LONG_PRESS_MS  600   // hold > 600ms = long press (cycle velocity)

// ── Color palette — deep indigo MIDI 2.0 theme ─────────────────────────────
#define M2_COL_BG       0x0000  // Black
#define M2_COL_HEADER   0x4814  // Deep indigo   — header
#define M2_COL_STATUS   0x0821  // Very dark blue — status row bg
#define M2_COL_DIVIDER  0x2945  // Dark slate     — divider / label
#define M2_COL_NOTE_BG  0x1082  // Dark navy      — last-note panel bg
#define M2_COL_WHITE    0xFFFF  // White
#define M2_COL_GREEN    0x07E0  // Green          — connected / active
#define M2_COL_ORANGE   0xFD20  // Orange         — waiting
#define M2_COL_CYAN     0x07FF  // Cyan           — NoteOn / bar fill
#define M2_COL_GRAY     0x4208  // Gray           — bar background / NoteOff
#define M2_COL_YELLOW   0xFFE0  // Yellow         — ControlChange
#define M2_COL_MAGENTA  0xF81F  // Magenta        — PitchBend
#define M2_COL_LIME     0x87E0  // Lime           — ProgramChange

// ── Layout — landscape 536 × 240 (setRotation(1) on RM67162 240×536) ───────
// CRITICAL: all x and w values used in drawing must be EVEN (RM67162 rule).
// All Y offsets and H values below are even for fillRect safety.
#define M2_W          536
#define M2_H          240
#define M2_MARGIN       4       // even

#define M2_Y_HEADER     0
#define M2_H_HEADER    30

#define M2_Y_STATUS    30       // WiFi + Peer side-by-side
#define M2_H_STATUS    18

#define M2_Y_DIV1      48       // "── LAST NOTE ──"
#define M2_H_DIV1      14

#define M2_Y_NOTE      62       // large note name + percentage
#define M2_H_NOTE      28

#define M2_Y_BAR       90       // 16-bit velocity bar
#define M2_H_BAR       16

#define M2_Y_VELVAL   106       // "vel = XXXXX  (16-bit)"
#define M2_H_VELVAL    14

#define M2_Y_DIV2     120       // "── EVENTS ──"
#define M2_H_DIV2      14

#define M2_Y_EVENTS   134       // scrolling event log
#define M2_H_EVENT     14
#define M2_N_EVENTS     6       // 6 × 14 = 84  (Y=134..218)

#define M2_Y_COUNTERS 218       // IN / OUT / VEL% counters
#define M2_H_COUNTERS  22       // 218 + 22 = 240 ✓

#endif // MAPPING_H
