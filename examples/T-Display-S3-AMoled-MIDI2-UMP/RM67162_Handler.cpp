#include "RM67162_Handler.h"
#include <cstdio>
#include <cstring>

RM67162_Handler display;

RM67162_Handler::RM67162_Handler()
    : _eventCount(0), _wifiConn(false), _peerActive(false)
{
    memset(_events,    0, sizeof(_events));
    memset(_wifiLabel, 0, sizeof(_wifiLabel));
    memset(_peerLabel, 0, sizeof(_peerLabel));
}

// ── private helpers ─────────────────────────────────────────────────────────

void RM67162_Handler::_fillRow(int y, int h, uint32_t bg) {
    _tft.fillRect(0, y, M2_W, h, bg);   // x=0, w=536 — both even ✓
}

void RM67162_Handler::_drawText(int x, int y, uint32_t fg, uint32_t bg,
                                uint8_t sz, const char* txt) {
    x = x & ~1;   // RM67162: ensure x is even
    _tft.setTextSize(sz);
    _tft.setTextColor(fg, bg);
    _tft.drawString(txt, x, y);
}

void RM67162_Handler::_drawDivider(int y, int h, const char* label) {
    _fillRow(y, h, M2_COL_BG);
    int lineY = y + h / 2;
    _tft.drawFastHLine(0, lineY, M2_W, M2_COL_DIVIDER);
    int lblW = (int)strlen(label) * 6;
    int lblX = ((M2_W - lblW) / 2) & ~1;   // ensure even x
    _drawText(lblX, y + 2, M2_COL_DIVIDER, M2_COL_BG, 1, label);
}

void RM67162_Handler::_redrawStatusRow() {
    _fillRow(M2_Y_STATUS, M2_H_STATUS, M2_COL_STATUS);
    int cy = M2_Y_STATUS + M2_H_STATUS / 2;

    // WiFi — left half
    uint32_t wc = _wifiConn ? M2_COL_GREEN : M2_COL_ORANGE;
    // Use fillRect instead of fillCircle to guarantee even x/w (RM67162)
    int dotY = cy - 4;
    _tft.fillRect(M2_MARGIN, dotY, 8, 8, wc);     // x=4 w=8 — both even ✓
    _drawText(M2_MARGIN + 12, M2_Y_STATUS + (M2_H_STATUS - 8) / 2,
              M2_COL_WHITE, M2_COL_STATUS, 1, _wifiLabel);

    // Peer — right half
    uint32_t pc = _peerActive ? M2_COL_GREEN : M2_COL_ORANGE;
    int peerDotX = (M2_W / 2 + M2_MARGIN) & ~1;   // ensure even
    _tft.fillRect(peerDotX, dotY, 8, 8, pc);
    _drawText(peerDotX + 12, M2_Y_STATUS + (M2_H_STATUS - 8) / 2,
              M2_COL_WHITE, M2_COL_STATUS, 1, _peerLabel);
}

// ── public API ───────────────────────────────────────────────────────────────

void RM67162_Handler::init() {
    // Power on AMOLED — GPIO 9 HIGH
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    delay(50);   // let power stabilize before SPI init

    _tft.init();
    _tft.setRotation(1);          // landscape 536×240
    _tft.setBrightness(200);      // AMOLED brightness via panel command (0-255)
    _tft.fillScreen(M2_COL_BG);
    // No backlight pin — AMOLED is self-emitting

    // ── Header ──────────────────────────────────────────────────────────────
    _fillRow(M2_Y_HEADER, M2_H_HEADER, M2_COL_HEADER);
    // RX dot — starts dim, uses fillRect for RM67162 even-x safety
    int dotY = M2_Y_HEADER + (M2_H_HEADER - 12) / 2;
    _tft.fillRect(M2_MARGIN, dotY, 12, 12, M2_COL_DIVIDER);  // x=4 w=12 — even ✓
    // Centred title
    const char* title = "MIDI 2.0  UMP";
    int titleW = (int)strlen(title) * 12;
    int titleX = ((M2_W - titleW) / 2) & ~1;
    _drawText(titleX, M2_Y_HEADER + (M2_H_HEADER - 16) / 2,
              M2_COL_WHITE, M2_COL_HEADER, 2, title);

    // ── Status row ──────────────────────────────────────────────────────────
    strncpy(_wifiLabel, "Connecting...", sizeof(_wifiLabel) - 1);
    strncpy(_peerLabel, "Waiting...",    sizeof(_peerLabel) - 1);
    _redrawStatusRow();

    // ── Last-note panel ─────────────────────────────────────────────────────
    _drawDivider(M2_Y_DIV1, M2_H_DIV1, " LAST NOTE ");
    setLastNote("", 0);

    // ── Events ──────────────────────────────────────────────────────────────
    _drawDivider(M2_Y_DIV2, M2_H_DIV2, " EVENTS ");
    _fillRow(M2_Y_EVENTS, M2_N_EVENTS * M2_H_EVENT, M2_COL_BG);

    // ── Counters ────────────────────────────────────────────────────────────
    _fillRow(M2_Y_COUNTERS, M2_H_COUNTERS, M2_COL_HEADER);
    setCounters(0, 0);
}

void RM67162_Handler::setWiFi(bool connected, const char* ip) {
    _wifiConn = connected;
    if (connected && ip && ip[0] != '\0')
        snprintf(_wifiLabel, sizeof(_wifiLabel), "WiFi %s", ip);
    else if (connected)
        strncpy(_wifiLabel, "WiFi OK", sizeof(_wifiLabel) - 1);
    else
        strncpy(_wifiLabel, "Connecting...", sizeof(_wifiLabel) - 1);
    _redrawStatusRow();
}

void RM67162_Handler::setPeer(bool active, const char* ip) {
    _peerActive = active;
    if (active && ip && ip[0] != '\0')
        snprintf(_peerLabel, sizeof(_peerLabel), "Peer %s", ip);
    else if (active)
        strncpy(_peerLabel, "Peer active", sizeof(_peerLabel) - 1);
    else
        strncpy(_peerLabel, "Waiting...", sizeof(_peerLabel) - 1);
    _redrawStatusRow();
}

void RM67162_Handler::pulseRxDot(bool on) {
    uint32_t col = on ? M2_COL_CYAN : M2_COL_DIVIDER;
    int dotY = M2_Y_HEADER + (M2_H_HEADER - 12) / 2;
    _tft.fillRect(M2_MARGIN, dotY, 12, 12, col);   // x=4 w=12 — even ✓
}

void RM67162_Handler::setLastNote(const char* noteOctave, uint16_t vel16) {
    bool empty = (!noteOctave || noteOctave[0] == '\0');

    // ── Note name (font 2, left) + percentage (font 2, right) ───────────────
    _fillRow(M2_Y_NOTE, M2_H_NOTE, M2_COL_NOTE_BG);
    if (empty) {
        const char* placeholder = "  ──";
        _drawText(M2_MARGIN, M2_Y_NOTE + 4,
                  M2_COL_DIVIDER, M2_COL_NOTE_BG, 2, placeholder);
    } else {
        _drawText(M2_MARGIN, M2_Y_NOTE + 4,
                  M2_COL_WHITE, M2_COL_NOTE_BG, 2, noteOctave);
        uint8_t pct = (uint8_t)((uint32_t)vel16 * 100 / 65535);
        char pctBuf[8];
        snprintf(pctBuf, sizeof(pctBuf), "%3d%%", pct);
        int pctW = (int)strlen(pctBuf) * 12;
        int pctX = (M2_W - M2_MARGIN - pctW) & ~1;   // ensure even x
        _drawText(pctX, M2_Y_NOTE + 4,
                  M2_COL_CYAN, M2_COL_NOTE_BG, 2, pctBuf);
    }

    // ── 16-bit velocity bar ─────────────────────────────────────────────────
    _fillRow(M2_Y_BAR, M2_H_BAR, M2_COL_NOTE_BG);
    int barX = M2_MARGIN;                          // 4 — even ✓
    int barW = (M2_W - 2 * M2_MARGIN) & ~1;       // 528 — even ✓
    int fill = empty ? 0 : (int)((uint32_t)barW * vel16 / 65535);
    fill = fill & ~1;                              // round down to even (RM67162)
    int rest = barW - fill;
    rest = rest & ~1;                              // ensure remainder is even too
    if (fill > 0)
        _tft.fillRect(barX,        M2_Y_BAR + 2, fill, M2_H_BAR - 4, M2_COL_CYAN);
    if (rest > 0)
        _tft.fillRect(barX + fill, M2_Y_BAR + 2, rest, M2_H_BAR - 4, M2_COL_GRAY);
    _tft.drawRect(barX,        M2_Y_BAR + 2, barW,  M2_H_BAR - 4, M2_COL_DIVIDER);

    // ── 16-bit value (centred) ───────────────────────────────────────────────
    _fillRow(M2_Y_VELVAL, M2_H_VELVAL, M2_COL_NOTE_BG);
    char valBuf[32];
    if (empty)
        snprintf(valBuf, sizeof(valBuf), "vel = ---  (16-bit)");
    else
        snprintf(valBuf, sizeof(valBuf), "vel = %-5u  (16-bit)", vel16);
    int valW = (int)strlen(valBuf) * 6;
    int valX = ((M2_W - valW) / 2) & ~1;          // ensure even x
    _drawText(valX, M2_Y_VELVAL + 2,
              M2_COL_WHITE, M2_COL_NOTE_BG, 1, valBuf);
}

void RM67162_Handler::pushEvent(uint32_t color, const char* line) {
    for (int i = M2_N_EVENTS - 1; i > 0; i--) _events[i] = _events[i - 1];
    _events[0].color = color;
    strncpy(_events[0].line, line, sizeof(_events[0].line) - 1);
    _events[0].line[sizeof(_events[0].line) - 1] = '\0';
    if (_eventCount < M2_N_EVENTS) _eventCount++;
    _redrawEvents();
}

void RM67162_Handler::_redrawEvents() {
    for (int i = 0; i < M2_N_EVENTS; i++) {
        int y = M2_Y_EVENTS + i * M2_H_EVENT;
        _fillRow(y, M2_H_EVENT, M2_COL_BG);
        if (i < _eventCount)
            _drawText(M2_MARGIN, y + 2, _events[i].color, M2_COL_BG, 1, _events[i].line);
    }
}

void RM67162_Handler::setCounters(int in, int out, int velPct) {
    _fillRow(M2_Y_COUNTERS, M2_H_COUNTERS, M2_COL_HEADER);
    char buf[24];

    // IN — left
    snprintf(buf, sizeof(buf), "IN: %d", in);
    _drawText(M2_MARGIN, M2_Y_COUNTERS + 6, M2_COL_WHITE, M2_COL_HEADER, 1, buf);

    // OUT — center
    snprintf(buf, sizeof(buf), "OUT: %d", out);
    int outX = (M2_W / 3 + M2_MARGIN) & ~1;       // ensure even x
    _drawText(outX, M2_Y_COUNTERS + 6, M2_COL_WHITE, M2_COL_HEADER, 1, buf);

    // VEL preset — right (only if >= 0)
    if (velPct >= 0) {
        snprintf(buf, sizeof(buf), "VEL: %d%%", velPct);
        int velW = (int)strlen(buf) * 6;
        int velX = (M2_W - M2_MARGIN - velW) & ~1;  // ensure even x
        _drawText(velX, M2_Y_COUNTERS + 6, M2_COL_CYAN, M2_COL_HEADER, 1, buf);
    }
}
