# 🎹 UART / DIN-5

Standard serial MIDI (31250 baud, 8N1) for connecting **vintage hardware** -- synthesizers, drum machines, mixers, and sequencers with DIN-5 connectors. Works on any ESP32.

---

## Serial MIDI Protocol

The original MIDI protocol (1983) uses asynchronous serial communication:

| Parameter | Value |
|-----------|-------|
| Baud rate | **31250 bps** (not 31.25 kHz -- it is 31250 bps) |
| Format | 8 bits, no parity, 1 stop bit (8N1) |
| Electrical level | 5V TTL with optical isolation (opto) |
| Connector | DIN-5 female (5 pins, but only 3 are used: GND, TX, RX) |

---

## DIN-5 Connector Pinout

```
        DIN-5 (front view of female connector)
             ___
           /     \
          / 4   5 \
         |  1   2  |
          \   3   /
           \_____/

DIN-5 pin 2 = GND (shield)
DIN-5 pin 4 = MIDI OUT (+5V current source)
DIN-5 pin 5 = MIDI IN (data)
```

---

## Hardware Circuit

### MIDI OUT (ESP32 → Synthesizer)

```
ESP32 GPIO TX ─── 220Ω ──► DIN-5 pin 5 (data)
3.3V / 5V     ─── 220Ω ──► DIN-5 pin 4 (source)
GND           ──────────► DIN-5 pin 2
```

!!! tip "Voltage level"
    MIDI OUT does not require an optocoupler. The two 220Ω resistors limit the current to the internal LED of the optocoupler in the receiving device.

### MIDI IN (Synthesizer → ESP32)

!!! warning "Isolation required"
    MIDI IN **must** use an optocoupler to electrically isolate the ESP32 from the instrument. Connecting directly can damage the ESP32 due to ground potential differences.

```
DIN-5 pin 5 ─── 220Ω ─── Optocoupler (LED)
DIN-5 pin 4 ─────────────── Optocoupler (anode)
DIN-5 pin 2 ──────────────────────────────── GND

Optocoupler (phototransistor):
  Collector ─── 3.3V
  Emitter ─── 1kΩ ─── ESP32 GPIO RX
```

**Recommended optocouplers:**

| Component | Notes |
|-----------|-------|
| **PC-900V** | Recommended by the MMA for MIDI |
| **6N138** | Most common, widely available |
| **TLP2361** | Fast, 5V, excellent choice |
| **H11L1** | Budget-friendly alternative |

### Complete Schematic

```
                    MIDI IN
DIN-5 pin 5 ─┬─ 220Ω ─► LED (+) of 6N138
DIN-5 pin 4 ─┘           LED (-) of 6N138
DIN-5 pin 2 ──────────── GND

6N138 output:
  VCC (pin 8) ─── 3.3V
  Vout (pin 6) ─── ESP32 RX (internal pull-up)
  GND (pin 4) ─── GND

                    MIDI OUT
ESP32 TX ─── 220Ω ─── DIN-5 pin 5
3.3V     ─── 220Ω ─── DIN-5 pin 4
GND      ─────────── DIN-5 pin 2
```

---

## Code

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"

UARTConnection uartMIDI;

void setup() {
    Serial.begin(115200);

    // Serial1: RX = GPIO 16, TX = GPIO 17
    uartMIDI.begin(Serial1, /*RX=*/16, /*TX=*/17);

    midiHandler.addTransport(&uartMIDI);
    midiHandler.begin();

    Serial.println("UART MIDI ready (31250 baud)");
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[DIN-5] %s %s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.channel0 + 1,
            ev.velocity7);
    }
}
```

---

## Multiple UARTs (ESP32-P4)

The ESP32-P4 has **5 hardware UARTs**, allowing multiple simultaneous DIN-5 ports:

```cpp
#include "src/UARTConnection.h"

UARTConnection uart1;  // MIDI IN/OUT port 1
UARTConnection uart2;  // MIDI IN/OUT port 2

void setup() {
    uart1.begin(Serial1, /*RX=*/16, /*TX=*/17);
    uart2.begin(Serial2, /*RX=*/18, /*TX=*/19);

    midiHandler.addTransport(&uart1);
    midiHandler.addTransport(&uart2);
    midiHandler.begin();
}
```

---

## Supported MIDI Messages

| Message | Support |
|---------|---------|
| NoteOn / NoteOff | ✅ |
| Control Change (CC) | ✅ |
| Program Change | ✅ |
| Pitch Bend | ✅ |
| Channel Pressure | ✅ |
| MIDI Clock / Start / Stop | ✅ (real-time messages) |
| Running Status | ✅ (processed automatically) |
| SysEx | ❌ (ignored) |

---

## Available Pins by Chip

| Chip | Hardware UARTs | Suggested pins (RX/TX) |
|------|----------------|------------------------|
| Classic ESP32 | 3 | UART1: 16/17, UART2: 4/2 |
| ESP32-S3 | 3 | UART1: 18/17, UART2: 19/20 |
| ESP32-S2 | 2 | UART1: 18/17 |
| ESP32-P4 | **5** | UART1: 16/17, UART2: 18/19, ... |
| ESP32-C3 | 2 | UART1: 4/5 |

!!! tip "Avoid GPIO 0"
    Do not use GPIO 0 for UART MIDI -- it is the boot button and can cause unexpected behavior.

---

## Examples

| Example | Description |
|---------|-------------|
| `UART-MIDI-Basic` | DIN-5 input → Serial Monitor |
| `P4-Dual-UART-MIDI` | Two UARTs on ESP32-P4 |

---

## DIN-5 ↔ WiFi Bridge

A classic use case: connect a vintage synthesizer to macOS via WiFi:

```
DIN-5 Synthesizer → ESP32 UART → MIDIHandler → RTP-MIDI → macOS Logic Pro
```

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"
#include "src/RTPMIDIConnection.h"

UARTConnection uartMIDI;
RTPMIDIConnection rtpMIDI;

void setup() {
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    uartMIDI.begin(Serial1, 16, 17);
    rtpMIDI.begin("Synth Bridge");

    midiHandler.addTransport(&uartMIDI);
    midiHandler.addTransport(&rtpMIDI);
    midiHandler.begin();
    // Done! DIN-5 ↔ WiFi automatic bridge
}
```

---

## Next Steps

- [RTP-MIDI →](rtp-midi.md) -- combine UART with WiFi Apple MIDI
- [ESP-NOW →](esp-now.md) -- DIN-5 ↔ ESP-NOW mesh bridge
- [UART Examples →](../exemplos/uart-basico.md) -- complete annotated sketch
