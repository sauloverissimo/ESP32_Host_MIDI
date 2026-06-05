/*
 * T-Display-S3-Piano-Flow - live music interpretation with midi2flow.
 *
 * Source-agnostic: a MIDI 1.0 keyboard or a MIDI 2.0/UMP device on the USB host
 * port both flow through the same interpreter (gingoduino's midi2flow). The
 * reading is shown live on the T-Display S3: the held notes, the current chord
 * named WITH inversion (CM, CM/E) and the last note's duration.
 *
 * USBMIDI2Connection negotiates MIDI 2.0 (Alt 1) when the device offers it and
 * falls back to MIDI 1.0 (Alt 0) otherwise:
 *   - MIDI 2.0 device: raw UMP arrives via the UMP callback and goes straight
 *     into the flow.
 *   - MIDI 1.0 device: MIDI bytes arrive via the MIDI callback, get converted
 *     to UMP MT2 (Midi1ToUmp), then into the same flow.
 * The display and transport setup follow the proven T-Display-S3-Piano-Debug
 * (display) and USB-Host-MIDI2 (transport) examples.
 *
 * Dependencies: ESP32_Host_MIDI v6.1+, Gingoduino 0.6.0+, LovyanGFX.
 */

#include <Arduino.h>
#include <LovyanGFX.h>
#include <USBMIDI2Connection.h>   // v6.1: USB Host with MIDI 1.0 + MIDI 2.0/UMP
#include "mapping.h"
#include "Midi1ToUmp.h"           // MIDI 1.0 bytestream -> UMP MT2
#include "FlowDisplayState.h"     // midi2flow -> held notes + chord+inversion + duration

// ── Global transport instance ─────────────────────────────────────────────────
USBMIDI2Connection usb;

// ── Flow interpreter (the brain) ──────────────────────────────────────────────
// g_flow holds the live reading; g_midi1 turns inbound MIDI 1.0 into UMP MT2 and
// feeds it. Declared in this order so the sink can see g_flow.
static FlowDisplayState g_flow;
static Midi1ToUmp       g_midi1(0, [](uint32_t w0, uint32_t /*w1*/) {
    const uint32_t w[1] = { w0 };
    g_flow.ingest(w, 1, millis());
});

// Source state for the status line (updated from the connection callbacks).
static volatile bool g_connected = false;

// ── LGFX - T-Display S3 (ST7789, 8-bit parallel) ────────────────────────────
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_Parallel8 _bus;
    lgfx::Panel_ST7789  _panel;
    lgfx::Light_PWM     _bl;
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
};

static const int SCREEN_W = 320;
static const int SCREEN_H = 170;

static LGFX        tft;
static LGFX_Sprite screen(&tft);

// ── Colours ──────────────────────────────────────────────────────────────────
#define COL_BG       0x0841
#define COL_HEADER   0x07FF   // cyan
#define COL_NOTEON   0x07E0   // bright green - flow held set
#define COL_INFO     0xBDF7   // light grey

// ── UMP callback - MIDI 2.0 device (Alt 1) ────────────────────────────────────
// One whole UMP packet per call, no CIN, no conversion. Straight into the flow.
static void onUMP(void* /*ctx*/, const uint32_t* words, uint8_t count) {
    g_flow.ingest(words, count, millis());
}

// ── MIDI callback - MIDI 1.0 device (Alt 0) ───────────────────────────────────
// data carries the USB-MIDI packet WITH the CIN byte (data[0]); BLE/raw MIDI has
// no CIN. Strip the CIN, drop system/realtime, then feed the channel-voice bytes
// to the converter (length follows the status: program change / channel pressure
// carry one data byte, everything else carries two).
static void onMidi1(void* /*ctx*/, const uint8_t* data, size_t len) {
    const uint8_t* m;
    size_t mlen;
    if      (len >= 4) { m = data + 1; mlen = 3; }   // USB-MIDI: skip CIN byte
    else if (len >= 2) { m = data;     mlen = len; } // BLE / raw MIDI
    else return;
    if (m[0] >= 0xF0) return;                        // system common / realtime: skip
    const uint8_t st  = m[0] & 0xF0;
    size_t        use = (st == 0xC0 || st == 0xD0) ? 2 : 3;
    if (use > mlen) use = mlen;
    g_midi1.feed(m, use);
}

static void onConnect(void* /*ctx*/)    { g_connected = true;  }
static void onDisconnect(void* /*ctx*/) { g_connected = false; }

// ── Mini-piano helpers (same logic as the Piano example) ──────────────────────
static const int MINI_KEYS    = 25;      // C to C, 2 octaves
static const int MINI_START   = 48;      // C3 (default view)
static const int MINI_H       = 20;
static const int MINI_Y       = SCREEN_H - MINI_H;
static const int MINI_WHITE_W = 12;
static const int MINI_BLACK_W = 8;
static const int MINI_BLACK_H = 12;

static const int S2W[12] = {0,-1,1,-1,2,3,-1,4,-1,5,-1,6};   // semitone to white index
static const int BLN[12] = {-1,0,-1,2,-1,-1,5,-1,7,-1,9,-1}; // black left neighbor

static bool isBk(int n) { return S2W[n % 12] < 0; }

static void drawMiniPiano(const bool notes[128]) {
    int x0 = (SCREEN_W - 15 * MINI_WHITE_W) / 2;
    screen.fillRect(0, MINI_Y, SCREEN_W, MINI_H, 0x0000);
    for (int n = MINI_START; n < MINI_START + MINI_KEYS; n++) {
        if (isBk(n)) continue;
        int wi = ((n - MINI_START) / 12) * 7 + S2W[n % 12];
        int x = x0 + wi * MINI_WHITE_W;
        uint16_t col = notes[n] ? 0x07FF : 0xFFFF;   // cyan if active
        screen.fillRect(x, MINI_Y + 1, MINI_WHITE_W - 1, MINI_H - 2, col);
    }
    for (int n = MINI_START; n < MINI_START + MINI_KEYS; n++) {
        if (!isBk(n)) continue;
        int nbSt = BLN[n % 12];
        int nbN  = (n / 12) * 12 + nbSt;
        int nbWi = ((nbN - MINI_START) / 12) * 7 + S2W[nbSt];
        int x = x0 + nbWi * MINI_WHITE_W + MINI_WHITE_W - MINI_BLACK_W / 2;
        uint16_t col = notes[n] ? 0xFBE0 : 0x2104;   // orange if active
        screen.fillRect(x, MINI_Y + 1, MINI_BLACK_W, MINI_BLACK_H, col);
    }
}

// ── Render ────────────────────────────────────────────────────────────────────
static void render() {
    screen.fillScreen(COL_BG);
    screen.setFont(&fonts::Font0);

    // Source line: which protocol the connected device negotiated.
    const char* src = !g_connected ? "no device"
                    : usb.isMIDI2() ? "MIDI 2.0"
                                    : "MIDI 1.0";
    screen.setTextColor(usb.isMIDI2() ? COL_HEADER : COL_INFO, COL_BG);
    char hdr[24];
    snprintf(hdr, sizeof hdr, "src  %s", src);
    screen.drawString(hdr, 4, 4);

    // Held set, as seen by the flow.
    char fl[56];
    int p = snprintf(fl, sizeof fl, "FLOW ");
    int fc = 0;
    for (int n = 0; n < 128 && p < (int)sizeof(fl) - 5; n++)
        if (g_flow.active(n)) { fc++; p += snprintf(fl + p, sizeof(fl) - p, "%d ", n); }
    if (!fc) snprintf(fl + p, sizeof(fl) - p, "-");
    screen.setTextColor(COL_NOTEON, COL_BG);
    screen.drawString(fl, 4, 16);

    screen.drawFastHLine(0, 28, SCREEN_W, 0x2945);

    // Chord with inversion - the headline reading from the flow.
    const char* ct = g_flow.chordText();
    screen.setFont(&fonts::Font4);
    screen.setTextColor(COL_HEADER, COL_BG);
    screen.setTextDatum(lgfx::middle_center);
    screen.drawString((ct && ct[0]) ? ct : "-", SCREEN_W / 2, 72);
    screen.setTextDatum(lgfx::top_left);

    // Last closed note's duration.
    char dur[28];
    snprintf(dur, sizeof dur, "last note  %lu ms", (unsigned long)g_flow.lastDurationMs());
    screen.setFont(&fonts::Font2);
    screen.setTextColor(COL_INFO, COL_BG);
    screen.drawString(dur, 4, 108);

    // Mini-piano driven by the flow's held set.
    bool held[128];
    for (int n = 0; n < 128; n++) held[n] = g_flow.active(n);
    drawMiniPiano(held);

    tft.startWrite();
    screen.pushSprite(0, 0);
    tft.endWrite();
}

// ── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);

    tft.init();
    tft.setRotation(2);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    screen.setColorDepth(16);
    if (!screen.createSprite(SCREEN_W, SCREEN_H)) {
        Serial.println("[Flow] ERROR: sprite alloc failed");
    }
    screen.setTextDatum(lgfx::top_left);

    usb.setUMPCallback(onUMP, nullptr);                       // MIDI 2.0 -> flow
    usb.setMidiCallback(onMidi1, nullptr);                    // MIDI 1.0 -> g_midi1 -> flow
    usb.setConnectionCallbacks(onConnect, onDisconnect, nullptr);
    if (!usb.begin()) {
        Serial.print("[Flow] USB host init failed: ");
        Serial.println(usb.getLastError());
    }

    render();
}

// ── Loop ─────────────────────────────────────────────────────────────────────
static uint32_t lastRenderMs = 0;

void loop() {
    usb.task();                               // drives USB host -> onUMP / onMidi1
    g_flow.poll();                            // drain the flow -> held / chord / duration

    const uint32_t now = millis();
    if (now - lastRenderMs >= 100) {
        lastRenderMs = now;
        render();
    }
}
