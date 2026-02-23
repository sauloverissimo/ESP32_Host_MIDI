#ifndef ST7789_HANDLER_H
#define ST7789_HANDLER_H

#include <LovyanGFX.h>
#include <string>
#include <cstdint>

// Color palette
#define OSC_COL_BG        0x0000  // Black background
#define OSC_COL_HEADER    0x780F  // Purple  — header / counter bar
#define OSC_COL_DIVIDER   0x4208  // Dark gray — divider line / label
#define OSC_COL_WHITE     0xFFFF  // White text
#define OSC_COL_GREEN     0x07E0  // Green  — WiFi connected
#define OSC_COL_RED       0xF800  // Red    — WiFi disconnected
#define OSC_COL_CYAN      0x07FF  // Cyan   — NoteOn
#define OSC_COL_GRAY      0x4208  // Gray   — NoteOff
#define OSC_COL_YELLOW    0xFFE0  // Yellow — ControlChange
#define OSC_COL_ORANGE    0xFD20  // Orange — PitchBend
#define OSC_COL_LIME      0x07E0  // Green  — ProgramChange

// Layout — portrait 170 × 320
#define OSC_Y_HEADER    0
#define OSC_H_HEADER   34
#define OSC_Y_WIFI     34
#define OSC_H_WIFI     22
#define OSC_Y_PORTS    56
#define OSC_H_PORTS    20
#define OSC_Y_DIVIDER  76
#define OSC_H_DIVIDER  14
#define OSC_Y_EVENTS   90
#define OSC_H_EVENT    20
#define OSC_N_EVENTS   10          // visible event rows
#define OSC_Y_COUNTERS (OSC_Y_EVENTS + OSC_N_EVENTS * OSC_H_EVENT)  // = 290
#define OSC_H_COUNTERS 30          // 290 + 30 = 320

#define OSC_DISPLAY_W  170
#define OSC_DISPLAY_H  320
#define OSC_MARGIN      4

class ST7789_Handler {
public:
    ST7789_Handler();

    // Hardware init + draws the static frame (header, divider, counters bar).
    void init();

    // Updates the WiFi / ports status rows.
    void setWifi(bool connected, const char* ip, int localPort, int remotePort);

    // Pushes a new event to the top of the scrolling list.
    // color : one of the OSC_COL_* constants.
    // line  : pre-formatted text (max ~27 chars for font size 1).
    void pushEvent(uint32_t color, const char* line);

    // Updates the IN / OUT counters in the bottom bar.
    void setCounters(int in, int out);

private:
    class LGFX : public lgfx::LGFX_Device {
    public:
        LGFX(void) {
            {
                auto cfg = _bus_instance.config();
                cfg.pin_wr = 8;  cfg.pin_rd = 9;  cfg.pin_rs = 7;
                cfg.pin_d0 = 39; cfg.pin_d1 = 40; cfg.pin_d2 = 41; cfg.pin_d3 = 42;
                cfg.pin_d4 = 45; cfg.pin_d5 = 46; cfg.pin_d6 = 47; cfg.pin_d7 = 48;
                _bus_instance.config(cfg);
                _panel_instance.setBus(&_bus_instance);
            }
            {
                auto cfg = _panel_instance.config();
                cfg.pin_cs  = 6;  cfg.pin_rst = 5;  cfg.pin_busy = -1;
                cfg.offset_rotation = 1;
                cfg.offset_x = 35;
                cfg.readable = false;  cfg.invert = true;
                cfg.rgb_order = false; cfg.dlen_16bit = false; cfg.bus_shared = false;
                cfg.panel_width  = OSC_DISPLAY_W;
                cfg.panel_height = OSC_DISPLAY_H;
                _panel_instance.config(cfg);
            }
            setPanel(&_panel_instance);
            {
                auto cfg = _light_instance.config();
                cfg.pin_bl = 38; cfg.invert = false;
                cfg.freq = 22000; cfg.pwm_channel = 7;
                _light_instance.config(cfg);
                _panel_instance.setLight(&_light_instance);
            }
        }
    private:
        lgfx::Bus_Parallel8  _bus_instance;
        lgfx::Panel_ST7789   _panel_instance;
        lgfx::Light_PWM      _light_instance;
    };

    LGFX _tft;

    struct EventEntry {
        uint32_t color;
        char     line[32];
    };
    EventEntry _events[OSC_N_EVENTS];
    int        _eventCount;

    void _drawHeader();
    void _redrawEvents();
    void _fillRow(int y, int h, uint32_t bg);
    void _drawText(int x, int y, uint32_t color, uint32_t bg, uint8_t size, const char* text);
};

extern ST7789_Handler display;

#endif // ST7789_HANDLER_H
