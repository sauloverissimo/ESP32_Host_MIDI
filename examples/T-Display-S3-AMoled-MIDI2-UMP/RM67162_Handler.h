#ifndef RM67162_HANDLER_H
#define RM67162_HANDLER_H

#include <LovyanGFX.h>
#include <cstdint>
#include "mapping.h"

class RM67162_Handler {
public:
    RM67162_Handler();
    void init();

    // WiFi + Peer status — displayed side-by-side in one row.
    void setWiFi(bool connected, const char* ip = "");
    void setPeer(bool active, const char* ip = "");

    // Header RX dot — pulses cyan on every received packet.
    void pulseRxDot(bool on);

    // "LAST NOTE" panel — large note name, 16-bit velocity bar, exact value.
    // Pass noteOctave="" to show placeholder "──".
    void setLastNote(const char* noteOctave, uint16_t vel16);

    // Scrolling event log.
    void pushEvent(uint32_t color, const char* line);

    // IN / OUT / VEL% counters bar.
    void setCounters(int in, int out, int velPct = -1);

private:
    class LGFX : public lgfx::LGFX_Device {
    public:
        LGFX() {
            // ── QSPI Bus ────────────────────────────────────────────
            { auto cfg = _bus.config();
              cfg.freq_write  = 75000000;
              cfg.freq_read   = 20000000;
              cfg.pin_sclk    = GPIO_NUM_47;
              cfg.pin_io0     = GPIO_NUM_18;   // MOSI
              cfg.pin_io1     = GPIO_NUM_7;    // MISO
              cfg.pin_io2     = GPIO_NUM_48;
              cfg.pin_io3     = GPIO_NUM_5;
              cfg.spi_host    = SPI2_HOST;
              cfg.spi_mode    = SPI_MODE0;
              cfg.dma_channel = SPI_DMA_CH_AUTO;
              _bus.config(cfg);
              _panel.setBus(&_bus); }
            // ── Panel RM67162 (240×536 AMOLED) ──────────────────────
            { auto cfg = _panel.config();
              cfg.pin_rst      = GPIO_NUM_17;
              cfg.pin_cs       = GPIO_NUM_6;
              cfg.panel_width  = 240;
              cfg.panel_height = 536;
              cfg.readable     = true;
              _panel.config(cfg); }
            setPanel(&_panel);
            // No Light_PWM — AMOLED brightness via setBrightness()
        }
    private:
        lgfx::Bus_SPI       _bus;
        lgfx::Panel_RM67162 _panel;
    };

    LGFX _tft;

    // 536px / 6px-per-char ≈ 88 chars max at font size 1
    struct EventEntry { uint32_t color; char line[90]; };
    EventEntry _events[M2_N_EVENTS];
    int        _eventCount;

    // Cached status strings to redraw only the changed half
    char _wifiLabel[24];
    char _peerLabel[24];
    bool _wifiConn;
    bool _peerActive;

    void _fillRow(int y, int h, uint32_t bg);
    void _drawText(int x, int y, uint32_t fg, uint32_t bg, uint8_t sz, const char* txt);
    void _drawDivider(int y, int h, const char* label);
    void _redrawStatusRow();
    void _redrawEvents();
};

extern RM67162_Handler display;

#endif // RM67162_HANDLER_H
