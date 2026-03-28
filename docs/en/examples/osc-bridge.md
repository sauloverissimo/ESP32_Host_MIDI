# 🎨 OSC Bridge

The `T-Display-S3-OSC` example implements a bidirectional OSC ↔ MIDI bridge: events from the USB keyboard are sent as OSC messages to the computer, and OSC messages from the computer are converted to MIDI and shown on the display.

---

## Required Hardware

| Component | Details |
|-----------|---------|
| Board | LilyGO T-Display-S3 |
| Keyboard | Any USB MIDI class-compliant device |
| Cable | USB-OTG |
| Network | WiFi (same network as the computer) |

---

## Prerequisites

```
Manage Libraries → "OSC" → OSC by Adrian Freed, Yotam Mann (CNMAT)
```

---

## Full Code

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "src/OSCConnection.h"
// Tools > USB Mode → "USB Host"

const char* WIFI_SSID       = "YourSSID";
const char* WIFI_PASSWORD   = "YourPassword";
IPAddress   REMOTE_IP       = IPAddress(192, 168, 1, 100);  // Computer IP (Max/MSP)
const int   LOCAL_OSC_PORT  = 8000;   // ESP32 listens here
const int   REMOTE_OSC_PORT = 9000;   // Max/MSP listens here

OSCConnection oscMIDI;

void setup() {
    Serial.begin(115200);

    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Sending OSC to: %s:%d\n",
        REMOTE_IP.toString().c_str(), REMOTE_OSC_PORT);
    Serial.printf("Receiving OSC on: :%d\n", LOCAL_OSC_PORT);

    oscMIDI.begin(LOCAL_OSC_PORT, REMOTE_IP, REMOTE_OSC_PORT);
    midiHandler.addTransport(&oscMIDI);
    midiHandler.begin();

    Serial.println("OSC ↔ MIDI bridge ready!");
}

void loop() {
    midiHandler.task();

    // MIDI events (from USB or OSC received from the computer)
    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[MIDI] %s %s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.channel0 + 1,
            ev.velocity7);

        // USB → OSC is automatic (oscMIDI.sendMidiMessage() is called internally)
    }
}
```

---

## OSC Addresses

| Direction | OSC Address | Arguments |
|-----------|-------------|-----------|
| ESP32 → Computer | `/midi/noteon` | `int channel, int note, int velocity` |
| ESP32 → Computer | `/midi/noteoff` | `int channel, int note, int velocity` |
| ESP32 → Computer | `/midi/cc` | `int channel, int controller, int value` |
| ESP32 → Computer | `/midi/pitchbend` | `int channel, int bend` |
| Computer → ESP32 | `/midi/noteon` | `int channel, int note, int velocity` |
| Computer → ESP32 | (all of the above) | -- |

---

## Max/MSP Patch

To receive MIDI from the ESP32 in Max/MSP:

```max
[udpreceive 9000]
      |
  [oscparse]
      |
  [route /midi/noteon /midi/noteoff /midi/cc]
  |              |              |
[unpack i i i] [unpack i i i] [unpack i i i]
  |    |   |
 ch  note vel
```

To send MIDI from Max/MSP to the ESP32:

```max
[pack i i i]       ← channel note velocity
      |
[oscformat /midi/noteon]
      |
[udpsend]
[connect 192.168.1.X 8000]   ← ESP32 IP
```

---

## Pure Data Patch

**Receive:**
```
[udpreceive 9000]
      |
  [import osc]
  [oscparse]
      |
  [route /midi/noteon]
```

**Send:**
```
[pack f f f]
      |
[oscformat /midi/noteon]
      |
[udpsend 192.168.1.X 8000]
```

---

## TouchOSC

Configure TouchOSC (iOS/Android):

1. Under **Connections → OSC**:
   - Host: ESP32 IP
   - Port (outgoing): 8000
   - Port (incoming): 9000
2. Create buttons with address `/midi/noteon` and arguments: channel, note, velocity

---

## Next Steps

- [OSC →](../transportes/osc.md) -- OSC transport details
- [RTP-MIDI →](rtp-midi-wifi.md) -- alternative for DAWs with AppleMIDI
