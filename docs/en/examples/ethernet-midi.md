# 🔗 Ethernet MIDI

The `Ethernet-MIDI` example implements a MIDI hub over wired Ethernet (W5500 SPI), using the AppleMIDI (RTP-MIDI) protocol. Ideal for studio racks and installations with managed networks.

---

## Required Hardware

| Component | Details |
|-----------|---------|
| ESP32 (any) | ESP32, S3, S2, P4... |
| W5500 module | SPI Ethernet (e.g., W5500 Mini) |
| Cable | RJ-45 Ethernet to switch/router |

---

## W5500 ↔ ESP32 Wiring

| W5500 | ESP32 GPIO | Notes |
|-------|-----------|-------|
| MOSI | 23 | SPI MOSI |
| MISO | 19 | SPI MISO |
| SCK | 18 | SPI Clock |
| CS (SCS) | 5 | Chip Select |
| RST | 4 | Reset (optional) |
| VCC | 3.3V | Power |
| GND | GND | Ground |

!!! tip "W5500 with ESP32-S3"
    For ESP32-S3, use the SPI2 pins: MOSI=11, MISO=13, SCK=12, CS=10.

---

## Prerequisites

```
Manage Libraries →
  "AppleMIDI" → Arduino-AppleMIDI-Library by lathoub (v3.x)
  "Ethernet" → Ethernet by Arduino (or W5500-compatible)
```

---

## Code

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/EthernetMIDIConnection.h"

// Unique MAC for the device (pick any unique value on the network)
static const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// SPI pins for W5500
#define ETH_CS_PIN 5

EthernetMIDIConnection ethMIDI;

void setup() {
    Serial.begin(115200);

    Serial.println("Starting Ethernet MIDI...");

    // Register BEFORE begin()
    midiHandler.addTransport(&ethMIDI);

    // Automatic DHCP
    ethMIDI.begin(MAC);

    // Or static IP:
    // ethMIDI.begin(MAC, IPAddress(192, 168, 1, 100));

    midiHandler.begin();

    Serial.printf("Ethernet IP: %s\n",
        Ethernet.localIP().toString().c_str());
    Serial.println("Configure Audio MIDI Setup on macOS:");
    Serial.printf("  Host: %s  Port: 5004\n",
        Ethernet.localIP().toString().c_str());
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[ETH-MIDI] %s %s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.channel0 + 1,
            ev.velocity7);
    }
}
```

---

## macOS Setup

Since the W5500 does not support mDNS, configure the IP manually:

1. **Audio MIDI Setup** → **Window → Show MIDI Studio**
2. Click on **Network**
3. Under "My Sessions" → **+** → create a new session
4. Under "Directory" → **+** → **Host: [ESP32 IP]** | **Port: 5004**
5. Select the session → **Connect**

---

## UART + Ethernet Bridge (Studio Rack)

Connect a vintage synthesizer via DIN-5 to the ESP32, which publishes it on the Ethernet network for the DAW:

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"
#include "src/EthernetMIDIConnection.h"

UARTConnection uartMIDI;
EthernetMIDIConnection ethMIDI;
const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    uartMIDI.begin(Serial1, 16, 17);     // DIN-5
    ethMIDI.begin(MAC, IPAddress(192, 168, 1, 50));

    midiHandler.addTransport(&uartMIDI);
    midiHandler.addTransport(&ethMIDI);
    midiHandler.begin();

    // Automatic bridge! Any MIDI from DIN-5 goes to Ethernet and vice versa.
}

void loop() { midiHandler.task(); }
```

---

## Typical Latency

| Environment | Latency | Jitter |
|-------------|---------|--------|
| Home WiFi | 8-25 ms | ±5 ms |
| **Wired Ethernet** | **2-8 ms** | **< 1 ms** |
| Managed Ethernet (QoS) | 1-3 ms | < 0.5 ms |

Ethernet is more predictable and consistent -- essential for precise musical timing in a studio.

---

## Next Steps

- [Ethernet MIDI →](../transportes/ethernet-midi.md) -- transport details
- [UART / DIN-5 →](../transportes/uart-din5.md) -- add a DIN-5 port
- [RTP-MIDI WiFi →](rtp-midi-wifi.md) -- wireless version with mDNS
