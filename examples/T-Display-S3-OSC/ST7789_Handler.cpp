#include "ST7789_Handler.h"
#include "mapping.h"
#include <cstdio>
#include <cstring>

ST7789_Handler display;

ST7789_Handler::ST7789_Handler() : _eventCount(0) {
    memset(_events, 0, sizeof(_events));
}

// ---- helpers -----------------------------------------------------------

void ST7789_Handler::_fillRow(int y, int h, uint32_t bg) {
    _tft.fillRect(0, y, OSC_DISPLAY_W, h, bg);
}

void ST7789_Handler::_drawText(int x, int y, uint32_t color, uint32_t bg,
                               uint8_t size, const char* text) {
    _tft.setTextSize(size);
    _tft.setTextColor(color, bg);
    _tft.drawString(text, x, y);
}

// ---- init --------------------------------------------------------------

void ST7789_Handler::_drawHeader() {
    _fillRow(OSC_Y_HEADER, OSC_H_HEADER, OSC_COL_HEADER);

    // Centered title with size-2 font (each char ~12 × 16 px)
    const char* title = "OSC  MIDI";
    int titleW = strlen(title) * 12;
    int titleX = (OSC_DISPLAY_W - titleW) / 2;
    int titleY = OSC_Y_HEADER + (OSC_H_HEADER - 16) / 2;
    _drawText(titleX, titleY, OSC_COL_WHITE, OSC_COL_HEADER, 2, title);
}

void ST7789_Handler::init() {
    _tft.init();
    _tft.setRotation(2);
    _tft.fillScreen(OSC_COL_BG);

    // Backlight on — extra guarantee for battery-powered operation
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // --- Static frame ---

    // Header
    _drawHeader();

    // WiFi row (placeholder until setWifi() is called)
    _fillRow(OSC_Y_WIFI, OSC_H_WIFI, OSC_COL_BG);
    _drawText(OSC_MARGIN, OSC_Y_WIFI + 7, OSC_COL_RED, OSC_COL_BG, 1, "WiFi   connecting...");

    // Ports row (blank until setWifi() is called)
    _fillRow(OSC_Y_PORTS, OSC_H_PORTS, OSC_COL_BG);

    // Divider row — horizontal line + label
    _fillRow(OSC_Y_DIVIDER, OSC_H_DIVIDER, OSC_COL_BG);
    int lineY = OSC_Y_DIVIDER + OSC_H_DIVIDER / 2;
    _tft.drawFastHLine(0, lineY, OSC_DISPLAY_W, OSC_COL_DIVIDER);
    const char* lbl = " EVENTS ";
    int lblW = strlen(lbl) * 6;
    int lblX = (OSC_DISPLAY_W - lblW) / 2;
    _drawText(lblX, OSC_Y_DIVIDER + 2, OSC_COL_DIVIDER, OSC_COL_BG, 1, lbl);

    // Events area — blank
    _fillRow(OSC_Y_EVENTS, OSC_N_EVENTS * OSC_H_EVENT, OSC_COL_BG);

    // Counters bar
    _fillRow(OSC_Y_COUNTERS, OSC_H_COUNTERS, OSC_COL_HEADER);
    setCounters(0, 0);
}

// ---- status rows -------------------------------------------------------

void ST7789_Handler::setWifi(bool connected, const char* ip,
                              int localPort, int remotePort) {
    // WiFi row
    _fillRow(OSC_Y_WIFI, OSC_H_WIFI, OSC_COL_BG);
    uint32_t dotColor = connected ? OSC_COL_GREEN : OSC_COL_RED;
    // Dot indicator
    _tft.fillCircle(OSC_MARGIN + 3, OSC_Y_WIFI + 11, 4, dotColor);
    // IP text
    char buf[40];
    if (connected) {
        snprintf(buf, sizeof(buf), "  WiFi  %s", ip);
    } else {
        snprintf(buf, sizeof(buf), "  WiFi  connecting...");
    }
    _drawText(OSC_MARGIN, OSC_Y_WIFI + 7, OSC_COL_WHITE, OSC_COL_BG, 1, buf);

    // Ports row
    _fillRow(OSC_Y_PORTS, OSC_H_PORTS, OSC_COL_BG);
    snprintf(buf, sizeof(buf), "  :%d  <->  :%d", localPort, remotePort);
    _drawText(OSC_MARGIN, OSC_Y_PORTS + 6, OSC_COL_DIVIDER, OSC_COL_BG, 1, buf);
}

// ---- events ------------------------------------------------------------

void ST7789_Handler::pushEvent(uint32_t color, const char* line) {
    // Shift entries down (newest goes to index 0)
    for (int i = OSC_N_EVENTS - 1; i > 0; i--) {
        _events[i] = _events[i - 1];
    }
    _events[0].color = color;
    strncpy(_events[0].line, line, sizeof(_events[0].line) - 1);
    _events[0].line[sizeof(_events[0].line) - 1] = '\0';

    if (_eventCount < OSC_N_EVENTS) _eventCount++;

    _redrawEvents();
}

void ST7789_Handler::_redrawEvents() {
    for (int i = 0; i < OSC_N_EVENTS; i++) {
        int y = OSC_Y_EVENTS + i * OSC_H_EVENT;
        _fillRow(y, OSC_H_EVENT, OSC_COL_BG);
        if (i < _eventCount) {
            // Small arrow prefix — newest is brighter, oldest fades (use same color)
            _drawText(OSC_MARGIN, y + 6, _events[i].color, OSC_COL_BG, 1, _events[i].line);
        }
    }
}

// ---- counters ----------------------------------------------------------

void ST7789_Handler::setCounters(int in, int out) {
    _fillRow(OSC_Y_COUNTERS, OSC_H_COUNTERS, OSC_COL_HEADER);

    char buf[24];
    // IN label + value (left half)
    snprintf(buf, sizeof(buf), "IN : %d", in);
    _drawText(OSC_MARGIN, OSC_Y_COUNTERS + 8, OSC_COL_WHITE, OSC_COL_HEADER, 1, buf);

    // OUT label + value (right half)
    snprintf(buf, sizeof(buf), "OUT: %d", out);
    int rightX = OSC_DISPLAY_W / 2 + 4;
    _drawText(rightX, OSC_Y_COUNTERS + 8, OSC_COL_WHITE, OSC_COL_HEADER, 1, buf);
}
