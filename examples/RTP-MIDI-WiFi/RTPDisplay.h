#ifndef RTP_DISPLAY_H
#define RTP_DISPLAY_H

// ── Didactic display for RTP-MIDI WiFi example ─────────────────────────────
// Shows WiFi/RTP-MIDI connection status, sequence info, and a mini piano
// with active key highlights. Designed for educational demonstrations.
//
// Layout (320×170 landscape):
//   ┌──────────────────────────────────────────────┐
//   │ [●] WiFi 192.168.1.42     [●] RTP: 1 peer   │
//   ├──────────────────────────────────────────────┤
//   │  Pop I-V-vi-IV                     PLAYING   │
//   │  Sending: C4  E4  G4                         │
//   │  MIDI: [0x90] [0x3C] [0x64]  NoteOn ch1     │
//   ├──────────────────────────────────────────────┤
//   │  [mini piano keys with highlights]           │
//   ├──────────────────────────────────────────────┤
//   │  Step [3/4]  ████████░░░░░░                  │
//   │  B1: Next Seq         B2: Play/Stop          │
//   └──────────────────────────────────────────────┘

#include <LovyanGFX.h>
#include "MusicSequences.h"

// ── Screen ──────────────────────────────────────────────────────────────────
static const int RD_SCREEN_W = 320;
static const int RD_SCREEN_H = 170;

// ── Colours — teal/green theme for WiFi (distinct from BLE cyan) ────────────
#define RD_COL_BG         0x1082   // dark grey-blue
#define RD_COL_HEADER     0x2945
#define RD_COL_DIVIDER    0x2945
#define RD_COL_TEXT       0xFFFF   // white
#define RD_COL_DIM        0x8410   // grey
#define RD_COL_ACCENT     0x3666   // teal
#define RD_COL_NOTE       0xFFE0   // yellow (note badge)
#define RD_COL_CHORD      0xFBE0   // orange (chord badge)
#define RD_COL_WIFI_ON    0x07E0   // green
#define RD_COL_WIFI_OFF   0xF800   // red
#define RD_COL_PEER_ON    0x3666   // teal
#define RD_COL_PEER_OFF   0xFFE0   // yellow (waiting)
#define RD_COL_PLAYING    0x07E0   // green
#define RD_COL_STOPPED    0x8410   // grey
#define RD_COL_BAR_BG     0x2945
#define RD_COL_BAR_FG     0x3666   // teal

// Mini piano
#define RD_COL_KEY_WHITE  0xFFFF
#define RD_COL_KEY_BLACK  0x0841
#define RD_COL_KEY_W_ACT  0x3666   // teal (active white key)
#define RD_COL_KEY_B_ACT  0xFBE0   // orange (active black key)
#define RD_COL_KEY_BORDER 0x0000

// ── Info passed to display each frame ───────────────────────────────────────
struct RTPInfo {
    // WiFi state
    bool     wifiConnected;
    char     ipAddress[16];        // "192.168.1.42"

    // RTP-MIDI state
    int      peerCount;            // connected RTP-MIDI sessions
    bool     rtpActive;            // peerCount > 0

    // Sequence
    const char* sequenceName;
    int      currentStep;
    int      totalSteps;
    bool     playing;

    // Current notes being sent
    uint8_t  currentNotes[6];
    uint8_t  currentNoteCount;
    uint8_t  currentVelocity;
    uint8_t  currentStatus;        // 0x90 NoteOn, 0x80 NoteOff, 0 = idle

    // Active notes (for mini piano) — local player + received
    const bool* activeNotes;       // pointer to bool[128]
};

// ── RTPDisplay class ────────────────────────────────────────────────────────
class RTPDisplay {
public:
    RTPDisplay();

    void init();
    void render(const RTPInfo& info);

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

    void _drawStatusBar(const RTPInfo& info);
    void _drawSequenceInfo(const RTPInfo& info);
    void _drawMidiBytes(const RTPInfo& info);
    void _drawMiniPiano(const bool notes[128]);
    void _drawProgressBar(const RTPInfo& info);
    void _drawControls(const RTPInfo& info);

    LGFX        _tft;
    LGFX_Sprite _screen;
};

extern RTPDisplay rtpDisplay;

#endif // RTP_DISPLAY_H
