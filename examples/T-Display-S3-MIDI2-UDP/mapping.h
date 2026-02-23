#ifndef MAPPING_H
#define MAPPING_H

// ── T-Display-S3 hardware ───────────────────────────────────────────────────
#define TFT_BL_PIN    38
#define PIN_POWER_ON  15

// ── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID  "YourSSID"
#define WIFI_PASS  "YourPassword"

// ── MIDI 2.0 UDP ports ───────────────────────────────────────────────────────
#define MIDI2_UDP_LOCAL_PORT   5006
#define MIDI2_UDP_REMOTE_PORT  5006

// ── Peer IP ──────────────────────────────────────────────────────────────────
// After flashing BOTH boards, open Serial (115200) and note each board's
// "My IP:" line.  Then set each board's PEER_IP to the OTHER board's address
// and re-flash.  Example:
//   Board A  →  PEER_IP = IPAddress(192,168,1,43)  (Board B's IP)
//   Board B  →  PEER_IP = IPAddress(192,168,1,42)  (Board A's IP)
#define PEER_IP  IPAddress(192, 168, 15, 9)   // Board A's IP  (swap for Board A firmware)

// ── Demo loop ────────────────────────────────────────────────────────────────
// Sends a note every N ms so you can test without a physical MIDI source.
// Set to 0 to disable.
#define DEMO_LOOP_MS  2000

// ── Color palette — deep indigo MIDI 2.0 theme ──────────────────────────────
#define M2_COL_BG       0x0000  // Black
#define M2_COL_HEADER   0x4814  // Deep indigo   — header
#define M2_COL_STATUS   0x0821  // Very dark blue — status row bg
#define M2_COL_DIVIDER  0x2945  // Dark slate    — divider / label
#define M2_COL_NOTE_BG  0x1082  // Dark navy     — last-note panel bg
#define M2_COL_WHITE    0xFFFF  // White
#define M2_COL_GREEN    0x07E0  // Green         — connected / active
#define M2_COL_ORANGE   0xFD20  // Orange        — waiting
#define M2_COL_CYAN     0x07FF  // Cyan          — NoteOn / bar fill
#define M2_COL_GRAY     0x4208  // Gray          — bar background / NoteOff
#define M2_COL_YELLOW   0xFFE0  // Yellow        — ControlChange
#define M2_COL_MAGENTA  0xF81F  // Magenta       — PitchBend
#define M2_COL_LIME     0x87E0  // Lime          — ProgramChange

// ── Layout — landscape 320 × 170 (setRotation(2) with offset_rotation=1) ───
#define M2_W          320
#define M2_H          170
#define M2_MARGIN       4

#define M2_Y_HEADER     0
#define M2_H_HEADER    24

#define M2_Y_STATUS    24   // WiFi + Peer side-by-side in one row
#define M2_H_STATUS    14

#define M2_Y_DIV1      38   // "── LAST NOTE ──"
#define M2_H_DIV1      12

#define M2_Y_NOTE      50   // large note name + percentage
#define M2_H_NOTE      20

#define M2_Y_BAR       70   // 16-bit velocity bar
#define M2_H_BAR       12

#define M2_Y_VELVAL    82   // "vel = XXXXX  (16-bit)"
#define M2_H_VELVAL    12

#define M2_Y_DIV2      94   // "── EVENTS ──"
#define M2_H_DIV2      12

#define M2_Y_EVENTS   106   // scrolling event log
#define M2_H_EVENT     12
#define M2_N_EVENTS     4   // 4 × 12 = 48  (Y=106..154)

#define M2_Y_COUNTERS 154   // IN / OUT counters
#define M2_H_COUNTERS  16   // 154 + 16 = 170 ✓

#endif // MAPPING_H
