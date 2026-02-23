#ifndef ST7789_HANDLER_H
#define ST7789_HANDLER_H

#include <LovyanGFX.h>
#include <cstdint>
#include "mapping.h"

class ST7789_Handler {
public:
    ST7789_Handler();
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

    // IN / OUT counters bar.
    void setCounters(int in, int out);

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

    LGFX _tft;

    struct EventEntry { uint32_t color; char line[52]; };
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

extern ST7789_Handler display;

#endif // ST7789_HANDLER_H
