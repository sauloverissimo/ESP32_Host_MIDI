#include "JamDisplay.h"
#include "mapping.h"
#include <cstring>

JamDisplay jamDisplay;

// ── Mini piano geometry ──────────────────────────────────────────────────────
static const int MINI_KEYS     = 25;      // C to C, 2 octaves
static const int MINI_START    = 48;      // C3
static const int MINI_WHITE    = 15;
static const int MINI_Y        = 90;
static const int MINI_H        = 50;
static const int MINI_WHITE_W  = 21;
static const int MINI_BLACK_W  = 12;
static const int MINI_BLACK_H  = 30;
static const int MINI_X0       = (JD_SCREEN_W - MINI_WHITE * MINI_WHITE_W) / 2;

// Semitone-to-white-key index (-1 = black key)
static const int S2W[12] = {0,-1,1,-1,2,3,-1,4,-1,5,-1,6};
// Black-key-to-left-neighbour (white key semitone)
static const int BLN[12] = {-1,0,-1,2,-1,-1,5,-1,7,-1,9,-1};

JamDisplay::JamDisplay()
    : _screen(&_tft)
{
}

void JamDisplay::init() {
    _tft.init();
    _tft.setRotation(2);
    _tft.setBrightness(255);
    _tft.fillScreen(TFT_BLACK);
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    _screen.setColorDepth(16);
    if (!_screen.createSprite(JD_SCREEN_W, JD_SCREEN_H)) {
        Serial.println("[JamDisplay] ERROR: sprite alloc failed!");
    }
    _screen.setTextDatum(lgfx::top_left);
}

void JamDisplay::render(const JamInfo& info) {
    _screen.fillScreen(JD_COL_BG);

    _drawStatusBar(info);
    _drawSequenceInfo(info);
    _drawMidiBytes(info);
    _drawDualPiano(info);
    _drawLegend();
    _drawProgressBar(info);
    _drawControls(info);

    _tft.startWrite();
    _screen.pushSprite(0, 0);
    _tft.endWrite();
}

// ── Status bar (ESP-NOW + MAC + peer) ───────────────────────────────────────

void JamDisplay::_drawStatusBar(const JamInfo& info) {
    _screen.fillRect(0, 0, JD_SCREEN_W, 16, JD_COL_HEADER);
    _screen.setFont(&fonts::Font0);

    // Title
    _screen.setTextColor(JD_COL_ACCENT, JD_COL_HEADER);
    _screen.drawString("ESP-NOW Jam", 4, 4);

    // Local MAC (last 3 bytes)
    char macBuf[16];
    snprintf(macBuf, sizeof(macBuf), "MAC:%02X%02X%02X",
             info.localMAC[0], info.localMAC[1], info.localMAC[2]);
    _screen.setTextColor(JD_COL_DIM, JD_COL_HEADER);
    _screen.drawString(macBuf, 120, 4);

    // Peer status
    if (info.peerActive) {
        _screen.fillCircle(270, 8, 4, JD_COL_PEER_ON);
        _screen.setTextColor(JD_COL_PEER_ON, JD_COL_HEADER);
        _screen.drawString("Peer OK", 278, 4);
    } else {
        _screen.fillCircle(270, 8, 4, JD_COL_PEER_OFF);
        _screen.setTextColor(JD_COL_PEER_OFF, JD_COL_HEADER);
        _screen.drawString("No Peer", 278, 4);
    }
}

// ── Sequence name + current notes ───────────────────────────────────────────

void JamDisplay::_drawSequenceInfo(const JamInfo& info) {
    int y = 20;

    // Sequence name
    _screen.setFont(&fonts::Font2);
    _screen.setTextColor(JD_COL_TEXT, JD_COL_BG);
    _screen.drawString(info.sequenceName ? info.sequenceName : "---", 4, y);

    // Playing/Stopped badge
    if (info.playing) {
        _screen.fillRoundRect(260, y, 56, 16, 3, JD_COL_PLAYING);
        _screen.setTextColor(0x0000, JD_COL_PLAYING);
        _screen.setFont(&fonts::Font0);
        _screen.drawString("PLAYING", 265, y + 4);
    } else {
        _screen.fillRoundRect(260, y, 56, 16, 3, JD_COL_STOPPED);
        _screen.setTextColor(0x0000, JD_COL_STOPPED);
        _screen.setFont(&fonts::Font0);
        _screen.drawString("STOPPED", 265, y + 4);
    }

    y += 22;

    // "Sending: C4  E4  G4"
    _screen.setFont(&fonts::Font2);

    if (info.currentNoteCount > 0 && info.currentStatus == 0x90) {
        _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
        _screen.drawString("Sending:", 4, y);

        int x = 68;
        for (int i = 0; i < info.currentNoteCount && i < 6; i++) {
            uint8_t note = info.currentNotes[i];
            char nbuf[8];
            snprintf(nbuf, sizeof(nbuf), "%s%d", midiNoteName(note), midiNoteOctave(note));

            int bw = strlen(nbuf) * 10 + 10;
            bool isBlack = (S2W[note % 12] < 0);
            uint16_t badgeCol = isBlack ? JD_COL_CHORD : JD_COL_NOTE;
            _screen.fillRoundRect(x, y - 1, bw, 17, 3, badgeCol);
            _screen.setTextColor(0x0000, badgeCol);
            _screen.drawString(nbuf, x + 5, y);
            x += bw + 4;
        }
    } else if (!info.playing) {
        _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
        _screen.drawString("Press B2 to play", 4, y);
    } else {
        _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
        _screen.drawString("...", 4, y);
    }
}

// ── MIDI bytes (educational hex display) ────────────────────────────────────

void JamDisplay::_drawMidiBytes(const JamInfo& info) {
    int y = 64;
    _screen.setFont(&fonts::Font0);

    if (info.currentNoteCount > 0 && info.currentStatus != 0) {
        _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
        _screen.drawString("MIDI:", 4, y);

        char hexBuf[8];
        int x = 38;

        // Status byte
        snprintf(hexBuf, sizeof(hexBuf), "0x%02X", info.currentStatus);
        _screen.fillRoundRect(x, y - 1, 34, 11, 2, 0x2945);
        _screen.setTextColor(JD_COL_ACCENT, 0x2945);
        _screen.drawString(hexBuf, x + 2, y);
        x += 38;

        // Data bytes
        for (int i = 0; i < info.currentNoteCount && i < 2; i++) {
            uint8_t val = (i == 0) ? info.currentNotes[0] : info.currentVelocity;
            snprintf(hexBuf, sizeof(hexBuf), "0x%02X", val);
            _screen.fillRoundRect(x, y - 1, 34, 11, 2, 0x2945);
            _screen.setTextColor(JD_COL_NOTE, 0x2945);
            _screen.drawString(hexBuf, x + 2, y);
            x += 38;
        }

        // Description
        x += 4;
        const char* desc = (info.currentStatus == 0x90) ? "NoteOn" : "NoteOff";
        _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
        char descBuf[32];
        snprintf(descBuf, sizeof(descBuf), "%s ch1", desc);
        _screen.drawString(descBuf, x, y);
    } else {
        _screen.setTextColor(0x2945, JD_COL_BG);
        _screen.drawString("MIDI: ---", 4, y);
    }

    // Divider
    _screen.drawFastHLine(0, 78, JD_SCREEN_W, JD_COL_DIVIDER);

    // Piano range labels
    _screen.setFont(&fonts::Font0);
    _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
    _screen.drawString("C3", MINI_X0 - 14, MINI_Y + 20);
    _screen.drawString("C5", MINI_X0 + MINI_WHITE * MINI_WHITE_W + 2, MINI_Y + 20);
}

// ── Dual-layer piano (local=cyan, remote=magenta, both=green) ───────────────

void JamDisplay::_drawDualPiano(const JamInfo& info) {
    // Background / border
    _screen.fillRect(MINI_X0 - 1, MINI_Y, MINI_WHITE * MINI_WHITE_W + 2, MINI_H, JD_COL_KEY_BORDER);

    const bool* local  = info.localNotes;
    const bool* remote = info.remoteNotes;

    // White keys
    for (int n = MINI_START; n < MINI_START + MINI_KEYS; n++) {
        int st = n % 12;
        if (S2W[st] < 0) continue;
        int wi = ((n - MINI_START) / 12) * 7 + S2W[st];
        int x = MINI_X0 + wi * MINI_WHITE_W;

        bool isLocal  = local  && local[n];
        bool isRemote = remote && remote[n];

        uint16_t col;
        if (isLocal && isRemote)  col = JD_COL_BOTH_W;
        else if (isLocal)         col = JD_COL_LOCAL_W;
        else if (isRemote)        col = JD_COL_REMOTE_W;
        else                      col = JD_COL_KEY_WHITE;

        _screen.fillRect(x, MINI_Y + 1, MINI_WHITE_W - 1, MINI_H - 2, col);
    }

    // Black keys
    for (int n = MINI_START; n < MINI_START + MINI_KEYS; n++) {
        int st = n % 12;
        if (S2W[st] >= 0) continue;
        int nbSt = BLN[st];
        int nbN  = (n / 12) * 12 + nbSt;
        int nbWi = ((nbN - MINI_START) / 12) * 7 + S2W[nbSt];
        int x = MINI_X0 + nbWi * MINI_WHITE_W + MINI_WHITE_W - MINI_BLACK_W / 2;

        bool isLocal  = local  && local[n];
        bool isRemote = remote && remote[n];

        uint16_t col;
        if (isLocal && isRemote)  col = JD_COL_BOTH_B;
        else if (isLocal)         col = JD_COL_LOCAL_B;
        else if (isRemote)        col = JD_COL_REMOTE_B;
        else                      col = JD_COL_KEY_BLACK;

        _screen.fillRect(x, MINI_Y + 1, MINI_BLACK_W, MINI_BLACK_H, col);
        _screen.drawRect(x, MINI_Y + 1, MINI_BLACK_W, MINI_BLACK_H, JD_COL_KEY_BORDER);
    }
}

// ── Colour legend ───────────────────────────────────────────────────────────

void JamDisplay::_drawLegend() {
    int y = MINI_Y + MINI_H + 2;
    _screen.setFont(&fonts::Font0);

    int x = MINI_X0;

    // Local
    _screen.fillCircle(x + 3, y + 3, 3, JD_COL_LOCAL_W);
    _screen.setTextColor(JD_COL_LOCAL_W, JD_COL_BG);
    _screen.drawString("Local", x + 10, y);
    x += 60;

    // Remote
    _screen.fillCircle(x + 3, y + 3, 3, JD_COL_REMOTE_W);
    _screen.setTextColor(JD_COL_REMOTE_W, JD_COL_BG);
    _screen.drawString("Remote", x + 10, y);
    x += 68;

    // Both
    _screen.fillCircle(x + 3, y + 3, 3, JD_COL_BOTH_W);
    _screen.setTextColor(JD_COL_BOTH_W, JD_COL_BG);
    _screen.drawString("Both", x + 10, y);
}

// ── Progress bar ────────────────────────────────────────────────────────────

void JamDisplay::_drawProgressBar(const JamInfo& info) {
    int y = 148;
    _screen.setFont(&fonts::Font0);
    _screen.setTextColor(JD_COL_DIM, JD_COL_BG);

    char stepBuf[16];
    snprintf(stepBuf, sizeof(stepBuf), "Step %d/%d", info.currentStep + 1, info.totalSteps);
    _screen.drawString(stepBuf, 4, y);

    int barX = 70;
    int barW = 240;
    int barH = 6;
    _screen.fillRoundRect(barX, y + 1, barW, barH, 2, JD_COL_BAR_BG);

    if (info.totalSteps > 0) {
        int fillW = (barW * (info.currentStep + 1)) / info.totalSteps;
        if (fillW < 4) fillW = 4;
        _screen.fillRoundRect(barX, y + 1, fillW, barH, 2, JD_COL_BAR_FG);
    }
}

// ── Controls help ───────────────────────────────────────────────────────────

void JamDisplay::_drawControls(const JamInfo& info) {
    int y = 158;
    _screen.setFont(&fonts::Font0);
    _screen.setTextColor(JD_COL_DIM, JD_COL_BG);
    _screen.drawString("B1: Next Seq", 4, y);

    _screen.setTextColor(info.playing ? JD_COL_PLAYING : JD_COL_DIM, JD_COL_BG);
    _screen.drawString(info.playing ? "B2: Stop" : "B2: Play", 240, y);
}
