# 📡 ESP-NOW

Ultra-low latency wireless MIDI between ESP32 units via Espressif's proprietary protocol. No router, no handshake, no pairing -- works on any ESP32.

---

## Features

| Aspect | Detail |
|--------|--------|
| Protocol | ESP-NOW (Espressif) |
| Physical | 2.4 GHz WiFi radio (P2P, no router) |
| Latency | 1-5 ms |
| Range | ~200 m (line of sight) |
| Mode | Broadcast or Unicast |
| Chips | Any ESP32, S2, S3, C3, C6 |
| Chips without ESP-NOW | ESP32-P4 (no WiFi radio) |

---

## How It Works

```mermaid
graph LR
    subgraph STAGE["🎸 Stage"]
        ESP1["ESP32 #1\n(Guitarist)"]
        ESP2["ESP32 #2\n(Keyboardist)"]
        ESP3["ESP32 #3\n(Bassist)"]
        HUB["ESP32 Hub\n(Central)"]
    end

    subgraph FOH["🎛️ FOH"]
        PC["Computer\n(DAW / USB)"]
    end

    ESP1 <-->|"ESP-NOW\n1-5 ms"| HUB
    ESP2 <-->|"ESP-NOW\n1-5 ms"| HUB
    ESP3 <-->|"ESP-NOW\n1-5 ms"| HUB
    HUB -->|"USB Device\nor RTP-MIDI"| PC

    style HUB fill:#3F51B5,color:#fff
    style STAGE fill:#1A237E,color:#fff,stroke:#283593
    style FOH fill:#1B5E20,color:#fff,stroke:#2E7D32
```

ESP-NOW uses the WiFi radio in peer-to-peer mode, without needing an access point. Multiple ESP32 units can communicate in broadcast (everyone receives from everyone) or unicast (point to point).

---

## Code -- Broadcast Mode

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/ESPNowConnection.h"

ESPNowConnection espNow;

void setup() {
    Serial.begin(115200);

    // WiFi channel must be the same on all ESP32 units in the group
    espNow.begin(/*channel=*/11);

    midiHandler.addTransport(&espNow);
    midiHandler.begin();

    Serial.println("ESP-NOW MIDI ready (broadcast)");
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[ESP-NOW] %s %s vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.velocity7);
    }

    // Send a note every 2 seconds (example)
    static unsigned long last = 0;
    if (millis() - last > 2000) {
        midiHandler.sendNoteOn(1, 60, 100);
        delay(200);
        midiHandler.sendNoteOff(1, 60, 0);
        last = millis();
    }
}
```

---

## Code -- Unicast Mode (specific peer)

```cpp
#include "src/ESPNowConnection.h"

ESPNowConnection espNow;

// MAC address of the target ESP32 (see Serial.println(WiFi.macAddress()))
uint8_t peerMAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

void setup() {
    espNow.begin(11);

    // Add a specific peer (unicast)
    espNow.addPeer(peerMAC);

    midiHandler.addTransport(&espNow);
    midiHandler.begin();
}
```

---

## Finding an ESP32's MAC Address

```cpp
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
    // Example: "AA:BB:CC:DD:EE:FF"
}
```

---

## Collaborative Jam -- 3 ESP32 Units

```mermaid
sequenceDiagram
    participant ESP1 as ESP32 #1 (USB Keyboard)
    participant ESP2 as ESP32 #2 (Pad)
    participant ESP3 as ESP32 #3 (Display)

    Note over ESP1,ESP3: All on the same ESP-NOW channel (11)
    Note over ESP1,ESP3: Broadcast — everyone receives from everyone

    ESP1->>ESP2: NoteOn C4 [ESP-NOW]
    ESP1->>ESP3: NoteOn C4 [ESP-NOW]
    ESP2->>ESP1: NoteOn kick drum [ESP-NOW]
    ESP2->>ESP3: NoteOn kick drum [ESP-NOW]

    Note over ESP3: Display shows\nall notes from the jam
```

---

## ESP-NOW + USB Host + BLE

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/ESPNowConnection.h"

ESPNowConnection espNow;

void setup() {
    // ESP-NOW
    espNow.begin(11);
    midiHandler.addTransport(&espNow);

    // USB Host + BLE started automatically
    MIDIHandlerConfig cfg;
    cfg.bleName = "Jam Node";
    midiHandler.begin(cfg);

    // Now USB keyboard + BLE + ESP-NOW are all active!
}

void loop() {
    midiHandler.task();

    // Events from any transport
    for (const auto& ev : midiHandler.getQueue()) {
        // Automatically forwarded to all others!
    }
}
```

---

## WiFi Channel Considerations

!!! warning "WiFi Channel"
    ESP-NOW and WiFi (station) must use the **same channel**. If the ESP32 is connected to a WiFi router, ESP-NOW will automatically use the router's channel. If there is no WiFi, you specify the channel in `espNow.begin(channel)`.

```cpp
// If using ESP-NOW together with WiFi (for RTP-MIDI):
WiFi.begin("ssid", "password");
while (WiFi.status() != WL_CONNECTED) delay(500);
// The channel is determined by the router -- DO NOT pass a channel to begin()
espNow.begin();  // uses the current WiFi channel

// If using only ESP-NOW (no WiFi):
espNow.begin(11);  // fixed channel 11 (1-13)
```

---

## Examples

| Example | Description |
|---------|-------------|
| `T-Display-S3-ESP-NOW-Jam` | Collaborative jam with display |
| `ESP-NOW-MIDI` | Basic ESP-NOW MIDI |

---

## Next Steps

- [BLE MIDI →](ble-midi.md) -- ~30 m range but compatible with iOS
- [RTP-MIDI →](rtp-midi.md) -- use WiFi with a router for greater range
- [ESP-NOW Examples →](../exemplos/esp-now-jam.md) -- full jam sketch
