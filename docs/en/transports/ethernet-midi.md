# 🔗 Ethernet MIDI

Same RTP-MIDI / AppleMIDI protocol as WiFi, but over **wired Ethernet**. Lower and more consistent latency -- ideal for studios and installations with managed networks.

---

## Features

| Aspect | Detail |
|--------|--------|
| Protocol | AppleMIDI / RTP-MIDI (RFC 6295) |
| Physical | Wired Ethernet (RJ-45) |
| Latency | 2-10 ms (more stable than WiFi) |
| Hardware | W5500 SPI Ethernet **or** ESP32-P4 (native MAC) |
| Requires | `lathoub/Arduino-AppleMIDI-Library` + `arduino-libraries/Ethernet` |

---

## Hardware

### Option 1: W5500 SPI Module (any ESP32)

The W5500 is a complete Ethernet chip with SPI interface. Works on **any ESP32**.

```
W5500 Module
  MOSI  → ESP32 GPIO 23
  MISO  → ESP32 GPIO 19
  SCK   → ESP32 GPIO 18
  CS    → ESP32 GPIO 5
  RST   → ESP32 GPIO 4
  VCC   → 3.3V
  GND   → GND
```

### Option 2: ESP32-P4 (native Ethernet MAC)

The ESP32-P4 has a native Ethernet MAC controller (IEEE 802.3). It only requires an external PHY (e.g., LAN8720):

```cpp
#if ESP32_HOST_MIDI_HAS_ETH_MAC
    // Native MAC available -- configure the PHY in sdkconfig
#endif
```

---

## Code

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/EthernetMIDIConnection.h"  // Requires AppleMIDI + Ethernet

EthernetMIDIConnection ethMIDI;
static const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    Serial.begin(115200);

    // Register before begin()
    midiHandler.addTransport(&ethMIDI);

    // Automatic DHCP
    ethMIDI.begin(MAC);  // (1)

    // Or static IP:
    // ethMIDI.begin(MAC, IPAddress(192, 168, 1, 100));  // (2)

    midiHandler.begin();

    Serial.printf("Ethernet MIDI: %s\n",
        Ethernet.localIP().toString().c_str());
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[ETH] %s %s vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.velocity7);
    }
}
```

**Notes:**

1. DHCP: The W5500 module obtains an IP automatically via DHCP
2. Static IP: Pass an `IPAddress` as the second argument

---

## Connecting on macOS

!!! note "mDNS not available on Ethernet with W5500"
    The standard Ethernet library does not support mDNS/Bonjour. You must configure the IP manually in Audio MIDI Setup.

### Step by step

1. Open **Audio MIDI Setup** → **Window → Show MIDI Studio**
2. Click **Network**
3. In "My Sessions", click **+** to create a session
4. In "Directory" → **+** → Enter the ESP32's IP and port **5004**
5. Select the session and click **Connect**

---

## WiFi vs. Ethernet -- Comparison

| Aspect | WiFi (RTP-MIDI) | Ethernet (W5500) |
|--------|-----------------|------------------|
| Cable | Wireless | RJ-45 |
| Average latency | 5-20 ms | **2-10 ms** |
| Jitter | Moderate | **Very low** |
| mDNS / Auto-discovery | ✅ | ❌ (manual IP) |
| Compatible chips | Any with WiFi | **Any ESP32** |
| Ideal for | Casual use / stage | **Studio / installation** |

---

## Practical Example -- Studio Rack

```
DIN-5 Synthesizer ──► ESP32 (UART) ──► Ethernet W5500 ──► Switch ──► macOS Logic Pro
```

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"
#include "src/EthernetMIDIConnection.h"

UARTConnection uartMIDI;
EthernetMIDIConnection ethMIDI;
const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    uartMIDI.begin(Serial1, 16, 17);
    ethMIDI.begin(MAC, IPAddress(192, 168, 1, 50));

    midiHandler.addTransport(&uartMIDI);
    midiHandler.addTransport(&ethMIDI);
    midiHandler.begin();
    // Automatic DIN-5 ↔ Ethernet bridge!
}

void loop() {
    midiHandler.task();
}
```

---

## Examples

| Example | Description |
|---------|-------------|
| `Ethernet-MIDI` | Basic Ethernet MIDI with W5500 |

---

## Next Steps

- [RTP-MIDI WiFi →](rtp-midi.md) -- wireless alternative with mDNS
- [UART / DIN-5 →](uart-din5.md) -- add a DIN-5 port to the rack
- [Supported Hardware →](../avancado/hardware.md) -- ESP32-P4 with native Ethernet
