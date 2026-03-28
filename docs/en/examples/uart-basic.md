# 🎹 UART MIDI Basic

The `UART-MIDI-Basic` example is the simplest sketch in the library -- serial MIDI via DIN-5 printed to the Serial Monitor. Ideal for testing your setup without USB-OTG hardware.

---

## Required Hardware

| Component | Details |
|-----------|---------|
| Board | Any ESP32 |
| MIDI IN | Optocoupler (6N138, PC-900V) + DIN-5 |
| MIDI OUT | 2x 220 ohm resistors + DIN-5 (optional) |

---

## MIDI IN Circuit

```
DIN-5 pin 5 ─── 220Ω ───► 6N138 (pin 2, LED +)
DIN-5 pin 4 ─────────────► 6N138 (pin 3, LED -)
DIN-5 pin 2 ─────────────► GND

6N138 pin 8 (VCC) ───► 3.3V
6N138 pin 6 (Vout) ──► ESP32 GPIO RX (internal pull-up)
6N138 pin 4 (GND) ───► GND
```

!!! warning "Isolation is mandatory"
    Connecting DIN-5 directly to a GPIO without an optocoupler can damage the ESP32. Always use optical isolation for MIDI IN.

---

## Full Code

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"

// MIDI pins
#define MIDI_RX_PIN 16   // Connected to the optocoupler
#define MIDI_TX_PIN 17   // Connected to DIN-5 MIDI OUT (via 220 ohm)

UARTConnection uartMIDI;

void setup() {
    Serial.begin(115200);
    Serial.println("UART MIDI Basic -- ESP32_Host_MIDI");
    Serial.println("-----------------------------------");

    // Start UART MIDI (31250 baud)
    uartMIDI.begin(Serial1, MIDI_RX_PIN, MIDI_TX_PIN);

    // Register and start the handler
    midiHandler.addTransport(&uartMIDI);
    midiHandler.begin();

    Serial.println("Ready! Waiting for MIDI via DIN-5...");
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        // Print formatted event
        if (ev.statusCode == MIDIHandler::NoteOn && ev.velocity7 > 0) {
            char noteBuf[8];
            Serial.printf("[NoteOn]  %-5s  ch=%d  vel=%3d  t=%lu ms\n",
                MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), ev.channel0 + 1, ev.velocity7, ev.timestamp);

            // Automatic NoteOff test after 100ms (TX)
            delay(100);
            midiHandler.sendNoteOff(ev.channel0 + 1, ev.noteNumber, 0);

        } else if (ev.statusCode == MIDIHandler::NoteOff) {
            char noteBuf[8];
            Serial.printf("[NoteOff] %-5s  ch=%d\n",
                MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), ev.channel0 + 1);

        } else if (ev.statusCode == MIDIHandler::ControlChange) {
            Serial.printf("[CC]      #%-3d = %3d  ch=%d\n",
                ev.noteNumber, ev.velocity7, ev.channel0 + 1);

        } else if (ev.statusCode == MIDIHandler::PitchBend) {
            Serial.printf("[PitchBend] %d  ch=%d\n",
                ev.pitchBend14, ev.channel0 + 1);
        }
    }
}
```

---

## Serial Monitor Output

When playing a C major chord on a MIDI keyboard:

```
UART MIDI Basic -- ESP32_Host_MIDI
-----------------------------------
Ready! Waiting for MIDI via DIN-5...
[NoteOn]  C4    ch=1  vel=100  t=1234 ms
[NoteOn]  E4    ch=1  vel= 95  t=1235 ms
[NoteOn]  G4    ch=1  vel=110  t=1236 ms
[NoteOff] C4    ch=1
[NoteOff] E4    ch=1
[NoteOff] G4    ch=1
[CC]      #7  = 127  ch=1
```

---

## TX Test (MIDI OUT)

To test MIDI OUT, send notes programmatically:

```cpp
// Add to loop() to play a C scale
static unsigned long lastNote = 0;
static int noteIdx = 0;
const int SCALE[] = {60, 62, 64, 65, 67, 69, 71, 72};  // C-D-E-F-G-A-B-C

if (millis() - lastNote > 500) {
    // Turn off previous note
    if (noteIdx > 0) {
        midiHandler.sendNoteOff(1, SCALE[(noteIdx - 1) % 8], 0);
    }
    // Turn on next note
    midiHandler.sendNoteOn(1, SCALE[noteIdx % 8], 100);
    noteIdx++;
    lastNote = millis();
}
```

---

## Pins by Board

| Board | RX (GPIO) | TX (GPIO) |
|-------|----------|----------|
| ESP32 DevKit | 16 | 17 |
| ESP32-S3 DevKit | 18 | 17 |
| LilyGO T-Display-S3 | 18 | 17 |
| ESP32-C3 | 4 | 5 |
| ESP32-P4 (UART1) | 16 | 17 |

---

## Next Steps

- [UART / DIN-5 →](../transportes/uart-din5.md) -- transport and circuit details
- [RTP-MIDI WiFi →](rtp-midi-wifi.md) -- combine UART with Apple MIDI
- [T-Display-S3 →](t-display-s3.md) -- add a display to the project
