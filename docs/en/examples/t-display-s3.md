# 🖥️ T-Display-S3 Piano

The `T-Display-S3-Piano` example turns the LilyGO T-Display-S3 into a live piano visualizer with a scrolling 25-key view, chord detection, and real-time MIDI information display.

---

## Required Hardware

| Component | Model |
|-----------|-------|
| Board | LilyGO T-Display-S3 |
| Display | ST7789 1.9" 170x320 (built-in) |
| Keyboard | Any USB MIDI class-compliant device |
| Cable | USB-OTG (micro to USB-A female) |

---

## What the Example Does

- **25-key piano roll**: shows keys from C4 to C6 as a horizontal piano
- **Auto-scrolling**: the 25-key window scrolls to show the active notes
- **Active notes**: pressed keys are highlighted
- **Event log**: last 8 events on screen (note, channel, velocity, timestamp)
- **USB information**: USB Host connection status

---

## Arduino IDE Settings

```
Board: "LilyGo T-Display-S3" (or "ESP32S3 Dev Module")
Tools > USB Mode → "USB Host"
Tools > PSRAM → "OPI PSRAM"
Upload Speed: 921600
```

---

## Example Structure

```
examples/T-Display-S3-Piano/
├── T-Display-S3-Piano.ino    ← Main sketch
├── mapping.h                  ← Hardware pins
├── ST7789_Handler.h           ← Display interface
└── ST7789_Handler.cpp         ← Display implementation
```

---

## mapping.h -- Pins

```cpp
// T-Display-S3 pinout
#define TFT_CS    6
#define TFT_RST   5
#define TFT_DC    7
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_BL    38   // Backlight
```

---

## Main Sketch (simplified)

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/GingoAdapter.h"   // Chord detection
#include "ST7789_Handler.h"
#include "mapping.h"
// Tools > USB Mode → "USB Host"

ST7789_Handler display;

void setup() {
    Serial.begin(115200);
    display.begin();
    display.clear();

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 20;
    cfg.chordTimeWindow = 50;
    midiHandler.begin(cfg);
}

void loop() {
    midiHandler.task();

    // Update display when notes change
    bool active[128] = {false};
    midiHandler.fillActiveNotes(active);
    display.updatePianoRoll(active);

    // Show chord name
    char chord[16] = "";
    if (midiHandler.getActiveNotesCount() > 0) {
        GingoAdapter::identifyLastChord(midiHandler, chord, sizeof(chord));
    }
    display.showChord(chord);

    // Event log
    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        display.addEvent(MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), ev.channel0 + 1, ev.velocity7);
    }

    display.render();
}
```

---

## Gallery

<div style="display:flex; gap:12px; flex-wrap:wrap; justify-content:center; margin:20px 0">
  <figure style="margin:0; text-align:center">
    <img src="https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/examples/T-Display-S3-Piano/images/pianno-MIDI-25.jpeg" width="300" alt="Piano Roll" style="border-radius:8px"/>
    <figcaption><em>25-key piano roll with active notes</em></figcaption>
  </figure>
  <figure style="margin:0; text-align:center">
    <img src="https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/examples/T-Display-S3-Gingoduino/images/gingo-duino-integration.jpeg" width="300" alt="Chord Names" style="border-radius:8px"/>
    <figcaption><em>Real-time chord detection</em></figcaption>
  </figure>
</div>

---

## Other T-Display-S3 Examples

| Example | Transport | What it shows |
|---------|-----------|---------------|
| `T-Display-S3` | USB Host | Active notes + log |
| `T-Display-S3-Queue` | USB Host | Event queue debug |
| `T-Display-S3-Piano` | USB Host | 25-key piano roll |
| `T-Display-S3-Piano-Debug` | USB Host | Piano + extended debug |
| `T-Display-S3-Gingoduino` | USB + BLE | Chords (Gingoduino) |
| `T-Display-S3-BLE-Sender` | BLE | BLE sequencer |
| `T-Display-S3-BLE-Receiver` | BLE | BLE receiver |
| `T-Display-S3-OSC` | OSC + WiFi | OSC bridge |
| `T-Display-S3-USB-Device` | BLE + USB Device | Dual bridge |
| `T-Display-S3-ESP-NOW-Jam` | ESP-NOW | Collaborative jam |

---

## Next Steps

- [GingoAdapter →](../funcionalidades/gingo-adapter.md) -- chord detection
- [USB Host →](../transportes/usb-host.md) -- USB transport details
- [UART Basic →](uart-basic.md) -- simplest example
