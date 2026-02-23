#ifndef JAM_DISPLAY_H
#define JAM_DISPLAY_H

// ── Dual-layer piano display for ESP-NOW MIDI Jam ───────────────────────────
// Shows local notes (cyan) and remote notes (magenta) on the same piano,
// with green overlap when both play the same note.
//
// Layout (320x170 landscape):
//   ┌──────────────────────────────────────────────────┐
//   │ ESP-NOW Jam   MAC: AA:BB:CC       Peer: OK  16px│
//   ├──────────────────────────────────────────────────┤
//   │  C Major Scale                      PLAYING 20px│
//   │  Sending: C4  E4  G4                       16px│
//   │  MIDI: [0x90] [0x3C] [0x64]  NoteOn ch1   12px│
//   ├──────────────────────────────────────────────────┤
//   │  [piano 25 keys: cyan/magenta/green]       50px│
//   │  ● Local  ● Remote  ● Both                 8px│
//   ├──────────────────────────────────────────────────┤
//   │  Step [3/15]  ████████░░░░░                 8px│
//   │  B1: Next Seq          B2: Play/Stop       12px│
//   └──────────────────────────────────────────────────┘

#include <LovyanGFX.h>
#include "MusicSequences.h"

// ── Screen ──────────────────────────────────────────────────────────────────
static const int JD_SCREEN_W = 320;
static const int JD_SCREEN_H = 170;

// ── Colours ─────────────────────────────────────────────────────────────────
#define JD_COL_BG         0x1082   // dark grey-blue
#define JD_COL_HEADER     0x2945
#define JD_COL_DIVIDER    0x2945
#define JD_COL_TEXT       0xFFFF   // white
#define JD_COL_DIM        0x8410   // grey
#define JD_COL_ACCENT     0x07FF   // cyan
#define JD_COL_NOTE       0xFFE0   // yellow (note badges)
#define JD_COL_CHORD      0xFBE0   // orange (chord badges)
#define JD_COL_PLAYING    0x07E0   // green
#define JD_COL_STOPPED    0x8410   // grey
#define JD_COL_BAR_BG     0x2945
#define JD_COL_BAR_FG     0x07FF   // cyan
#define JD_COL_PEER_ON    0x07E0   // green
#define JD_COL_PEER_OFF   0xF800   // red

// Piano key colours — 3 states per key
#define JD_COL_KEY_WHITE  0xFFFF
#define JD_COL_KEY_BLACK  0x0841
#define JD_COL_KEY_BORDER 0x0000
// Local (this board plays)
#define JD_COL_LOCAL_W    0x07FF   // cyan (white key active)
#define JD_COL_LOCAL_B    0x001F   // blue  (black key active)
// Remote (other board plays)
#define JD_COL_REMOTE_W   0xF81F   // magenta (white key active)
#define JD_COL_REMOTE_B   0x780F   // purple  (black key active)
// Both (same note on both boards)
#define JD_COL_BOTH_W     0x07E0   // green (white key active)
#define JD_COL_BOTH_B     0x03E0   // dark green (black key active)

// ── Info struct passed to display each frame ────────────────────────────────
struct JamInfo {
    // ESP-NOW state
    bool     peerActive;        // received data recently
    uint8_t  localMAC[3];       // last 3 bytes of local MAC (for display)

    // Sequence
    const char* sequenceName;
    int      currentStep;
    int      totalSteps;
    bool     playing;

    // Current notes being sent
    uint8_t  currentNotes[6];
    uint8_t  currentNoteCount;
    uint8_t  currentVelocity;
    uint8_t  currentStatus;     // 0x90 NoteOn, 0x80 NoteOff, 0 = idle

    // Piano note arrays
    const bool* localNotes;     // pointer to bool[128] — this board
    const bool* remoteNotes;    // pointer to bool[128] — other board
};

// ── JamDisplay class ────────────────────────────────────────────────────────
class JamDisplay {
public:
    JamDisplay();

    void init();
    void render(const JamInfo& info);

private:
    class LGFX : public lgfx::LGFX_Device {
    public:
        LGFX() {
            { auto cfg = _bus.config();
              cfg.pin_wr=8; cfg.pin_rd=9; cfg.pin_rs=7;
              cfg.pin_d0=39; cfg.pin_d1=40; cfg.pin_d2=41; cfg.pin_d3=42;
              cfg.pin_d4=45; cfg.pin_d5=46; cfg.pin_d6=47; cfg.pin_d7=48;
              _bus.config(cfg); _panel.setBus(&_bus); }
            { auto cfg = _panel.config();
              cfg.pin_cs=6; cfg.pin_rst=5; cfg.pin_busy=-1;
              cfg.offset_rotation=1; cfg.offset_x=35;
              cfg.readable=false; cfg.invert=true;
              cfg.rgb_order=false; cfg.dlen_16bit=false; cfg.bus_shared=false;
              cfg.panel_width=170; cfg.panel_height=320;
              _panel.config(cfg); }
            setPanel(&_panel);
            { auto cfg = _bl.config();
              cfg.pin_bl=38; cfg.invert=false; cfg.freq=22000; cfg.pwm_channel=7;
              _bl.config(cfg); _panel.setLight(&_bl); }
        }
    private:
        lgfx::Bus_Parallel8 _bus;
        lgfx::Panel_ST7789  _panel;
        lgfx::Light_PWM     _bl;
    };

    void _drawStatusBar(const JamInfo& info);
    void _drawSequenceInfo(const JamInfo& info);
    void _drawMidiBytes(const JamInfo& info);
    void _drawDualPiano(const JamInfo& info);
    void _drawLegend();
    void _drawProgressBar(const JamInfo& info);
    void _drawControls(const JamInfo& info);

    LGFX        _tft;
    LGFX_Sprite _screen;
};

extern JamDisplay jamDisplay;

#endif // JAM_DISPLAY_H
