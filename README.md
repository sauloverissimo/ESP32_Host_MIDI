# ESP32\_Host\_MIDI

## The universal MIDI host hub for ESP32. One clean event API for USB, BLE, WiFi, Ethernet, ESP-NOW, OSC, and DIN-5.

![ESP32_Host_MIDI](https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/ESP32_Host_MIDI.png)

*Arduino library, multi-transport, MIDI 1.0 and 2.0, MIT.* Eight transports, one event queue.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![CI](https://github.com/sauloverissimo/ESP32_Host_MIDI/actions/workflows/ci.yml/badge.svg)](https://github.com/sauloverissimo/ESP32_Host_MIDI/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/sauloverissimo/ESP32_Host_MIDI)](https://github.com/sauloverissimo/ESP32_Host_MIDI/releases)
[![Platform](https://img.shields.io/badge/Platform-Arduino%20%7C%20PlatformIO%20%7C%20ESP--IDF-00979D.svg)](#installation)
[![MIDI](https://img.shields.io/badge/MIDI-1.0%20%7C%202.0-blueviolet.svg)](https://midi.org/specifications)
[![Sponsor](https://img.shields.io/badge/sponsor-%E2%9D%A4-pink.svg)](https://github.com/sponsors/sauloverissimo)

**Language:** [🇧🇷 Português (Brasil)](README-PT-BR.md)

---

## Overview

ESP32\_Host\_MIDI turns an ESP32 into a multi-protocol MIDI hub. Connect a USB MIDI keyboard over USB Host, receive notes from an iPhone over BLE, bridge a DAW over WiFi with RTP-MIDI (Apple MIDI), talk to Max/MSP over OSC, reach vintage DIN-5 gear over serial, and link several ESP32 boards over ESP-NOW. Every transport delivers into a single `MIDIHandler` event queue and shares one send API.

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;

void setup() {
    Serial.begin(115200);
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}

void loop() {
    midiHandler.task();
    for (const auto& ev : midiHandler.getQueue()) {
        char buf[8];
        Serial.printf("%-12s %-4s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, buf, sizeof(buf)),
            ev.channel0 + 1, ev.velocity7);
    }
}
```

---

## Transports

| Transport | Protocol | Physical | Latency | Requires |
|-----------|----------|----------|---------|----------|
| [USB Host](#usb-host) | USB MIDI 1.0 | USB-OTG cable | < 1 ms | ESP32-S3 / S2 / P4 |
| [USB Host MIDI 2.0](#usb-host-midi-20) | USB MIDI 2.0 (UMP) | USB-OTG cable | < 1 ms | ESP32-S3 / S2 / P4 |
| [BLE MIDI](#ble-midi) | BLE MIDI 1.0 | Bluetooth LE | 3-15 ms | Any ESP32 with BT |
| [ESP-NOW](#esp-now) | ESP-NOW | 2.4 GHz radio | 1-5 ms | Any ESP32 |
| [RTP-MIDI](#rtp-midi-apple-midi) | AppleMIDI / RFC 6295 | WiFi UDP | 5-20 ms | Any ESP32 with WiFi |
| [Ethernet MIDI](#ethernet-midi) | AppleMIDI / RFC 6295 | Wired (W5500 / native) | 2-10 ms | W5500 SPI or ESP32-P4 |
| [OSC](#osc) | Open Sound Control | WiFi UDP | 5-15 ms | Any ESP32 with WiFi |
| [UART / DIN-5](#uart--din-5) | Serial MIDI 1.0 | DIN-5 connector | < 1 ms | Any ESP32 |

Every transport implements the same `MIDITransport` interface and registers with one line: `midiHandler.addTransport(&t)`.

---

## Quick start

Since v6.0 transports are explicit: include the header you need, declare the transport, register it with `addTransport()`, and call its `begin()`. See [`docs/migration-v6.md`](docs/migration-v6.md) if you are upgrading from v5.x.

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>   // include only the transports you use
// Arduino IDE: Tools > USB Mode > "USB Host"

USBConnection usbHost;

void setup() {
    Serial.begin(115200);
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}

void loop() {
    midiHandler.task();
    for (const auto& ev : midiHandler.getQueue()) {
        char buf[8];
        Serial.println(MIDIHandler::noteWithOctave(ev.noteNumber, buf, sizeof(buf)));
    }
}
```

### Reading events

Each `MIDIEventData` in the queue exposes both MIDI 1.0 and MIDI 2.0 resolution:

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    ev.statusCode;   // MIDI_NOTE_ON | MIDI_NOTE_OFF | MIDI_CONTROL_CHANGE | ...
    ev.channel0;     // 0-15 (MIDI spec convention)
    ev.noteNumber;   // 0-127 (controller number for CC)
    ev.velocity7;    // 0-127 (MIDI 1.0)
    ev.velocity16;   // 0-65535 (MIDI 2.0, scaled)
    ev.pitchBend14;  // 0-16383 (center = 8192)
    ev.pitchBend32;  // 0-0xFFFFFFFF (MIDI 2.0, center = 0x80000000)
    ev.chordIndex;   // groups simultaneous notes
    ev.timestamp;    // millis() at arrival

    // Static helpers (zero allocation):
    MIDIHandler::noteName(ev.noteNumber);    // "C", "C#", "D" ...
    MIDIHandler::noteOctave(ev.noteNumber);  // -1 to 9
    MIDIHandler::statusName(ev.statusCode);  // "NoteOn", "ControlChange" ...
}
```

---

## Sending and bridging

Send from any transport with the unified API. `send*` tries each registered transport in order and stops at the first that accepts the message (first-wins). Channel is 1 to 16.

```cpp
midiHandler.sendNoteOn(1, 60, 100);
midiHandler.sendControlChange(1, 64, 127);
midiHandler.sendPitchBend(1, 0);           // -8192 to +8191, center = 0
```

MIDI is **not** routed between transports automatically. Each transport delivers its incoming MIDI into the shared event queue; your `loop()` decides what to forward. To bridge two transports, read the queue and resend to the target transport:

```cpp
// Forward note/CC from any input to a target transport.
static int lastIndex = 0;
for (const auto& ev : midiHandler.getQueue()) {
    if (ev.index <= lastIndex) continue;
    lastIndex = ev.index;
    uint8_t msg[3] = { uint8_t(ev.statusCode | ev.channel0), ev.noteNumber, ev.velocity7 };
    target.sendMidiMessage(msg, 3);
}
```

| Bridge | Path |
|--------|------|
| USB keyboard to WiFi | USB keyboard -> ESP32 -> RTP-MIDI -> macOS |
| Modern to legacy | macOS -> RTP-MIDI -> ESP32 -> DIN-5 -> 1980s drum machine |
| Wireless stage mesh | ESP-NOW nodes -> ESP32 hub -> RTP-MIDI -> FOH computer |
| Creative software | Max/MSP OSC -> ESP32 -> BLE -> iPad instrument |

---

## Transport reference

### USB Host

Connects any class-compliant USB MIDI device (keyboards, pads, interfaces, controllers) directly to the ESP32 USB-OTG port. No hub, no driver, no OS configuration.

**Boards:** ESP32-S3, S2, P4 · **Arduino IDE:** `Tools > USB Mode > "USB Host"`

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;

void setup() {
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}
```

For a full host example that also decodes MIDI 2.0, see the `USB-Host-MIDI2` example.

### USB Host MIDI 2.0

Native USB MIDI 2.0 / UMP. `USBMIDI2Connection` extends `USBConnection`, scans the device configuration descriptor for Alt 0 (MIDI 1.0) and Alt 1 (MIDI 2.0), and prefers MIDI 2.0 when available, falling back to MIDI 1.0. After negotiation it runs read-only UMP discovery (Endpoint Info, Function Block Info). Raw 32-bit UMP words are delivered one whole packet at a time, reassembled across USB transfers.

```cpp
#include <USBMIDI2Connection.h>

USBMIDI2Connection usb;

void onUMP(void*, const uint32_t* words, uint8_t count) {
    // Raw UMP words, MIDI 2.0 native resolution.
}

void setup() {
    usb.setUMPCallback(onUMP, nullptr);
    usb.setMidiCallback(onMidi, nullptr);   // MIDI 1.0 fallback
    usb.begin();
}
```

Query the negotiated capabilities:

```cpp
if (usb.isMIDI2() && usb.isNegotiated()) {
    const auto& ep = usb.getEndpointInfo();
    Serial.printf("UMP v%d.%d, %d function blocks\n",
        ep.umpVersionMajor, ep.umpVersionMinor, ep.numFunctionBlocks);
}
```

**Boards:** ESP32-S3, S2, P4 · **Examples:** `USB-Host-MIDI2`, `T-Display-S3-Piano-Flow`

### BLE MIDI

The ESP32 advertises as a BLE MIDI 1.0 peripheral. macOS (**Audio MIDI Setup > Bluetooth**), iOS (GarageBand, AUM, Loopy, Moog), and Android connect with no pairing ritual. Central (scanner) mode connects to another BLE MIDI device.

**Boards:** Any ESP32 with Bluetooth · **Range:** ~30 m · **Latency:** 3-15 ms

```cpp
#include <ESP32_Host_MIDI.h>
#include <BLEConnection.h>

BLEConnection ble;

void setup() {
    ble.begin("ESP32 MIDI");
    midiHandler.addTransport(&ble);
    midiHandler.begin();
}
```

**Examples:** `T-Display-S3-BLE-Sender`, `T-Display-S3-BLE-Receiver`

### ESP-NOW

Low-latency wireless MIDI between ESP32 boards over Espressif peer-to-peer radio. No WiFi router, no handshake, no pairing. Broadcast (every board hears everyone) or unicast.

**Boards:** Any ESP32 · **Range:** ~200 m open air · **Infrastructure:** none

```cpp
#include <ESP32_Host_MIDI.h>
#include <ESPNowConnection.h>

ESPNowConnection espNow;

void setup() {
    espNow.begin();
    midiHandler.addTransport(&espNow);
    midiHandler.begin();
}
```

**Examples:** `T-Display-S3-ESP-NOW-Jam`

### RTP-MIDI (Apple MIDI)

**Apple MIDI** (RTP-MIDI, RFC 6295) over WiFi UDP. macOS and iOS discover the ESP32 over **mDNS Bonjour** and show it in **Audio MIDI Setup > Network** with no manual configuration. Works with Logic Pro, GarageBand, Ableton, and any CoreMIDI app.

**Requires:** `lathoub/Arduino-AppleMIDI-Library` v3.x

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include <RTPMIDIConnection.h>

RTPMIDIConnection rtpMIDI;

void setup() {
    WiFi.begin("YourSSID", "YourPassword");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    rtpMIDI.begin("ESP32 MIDI");
    midiHandler.addTransport(&rtpMIDI);
    midiHandler.begin();
}
```

**Examples:** `RTP-MIDI-WiFi`

### Ethernet MIDI

The same RTP-MIDI / AppleMIDI protocol over a wired W5500 SPI Ethernet module or the ESP32-P4 native Ethernet MAC. Lower and more consistent latency than WiFi. Ideal for studio racks and live venues.

**Requires:** `lathoub/Arduino-AppleMIDI-Library` v3.x and the Arduino `Ethernet` library

```cpp
#include <ESP32_Host_MIDI.h>
#include <EthernetMIDIConnection.h>

EthernetMIDIConnection ethMIDI;
static const uint8_t MAC[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    ethMIDI.begin(MAC);   // DHCP; pass a static IPAddress as second arg for a fixed IP
    midiHandler.addTransport(&ethMIDI);
    midiHandler.begin();
}
```

**Examples:** `Ethernet-MIDI`

### OSC

Bidirectional **OSC to MIDI** bridge over WiFi UDP. Receives OSC from Max/MSP, Pure Data, SuperCollider, and TouchOSC and converts it to MIDI events, and sends every MIDI event out as OSC.

**Address map:** `/midi/noteon`, `/midi/noteoff`, `/midi/cc`, `/midi/pc`, `/midi/pitchbend`, `/midi/aftertouch`
**Requires:** `CNMAT/OSC` library

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include <OSCConnection.h>

OSCConnection oscMIDI;

void setup() {
    WiFi.begin("YourSSID", "YourPassword");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    oscMIDI.begin(8000, IPAddress(192, 168, 1, 100), 9000);
    midiHandler.addTransport(&oscMIDI);
    midiHandler.begin();
}
```

**Examples:** `T-Display-S3-OSC`

### UART / DIN-5

Standard serial MIDI (31250 baud, 8N1) for vintage hardware: synthesizers, drum machines, mixers, sequencers, anything with a DIN-5 connector. Supports running status, real-time messages (Clock, Start, Stop), and multiple UART ports (the ESP32-P4 has five hardware UARTs).

**Hardware:** TX to DIN-5 pin 5 via 220 Ohm; PC-900V / 6N138 optocoupler on RX to DIN-5 pin 4

```cpp
#include <ESP32_Host_MIDI.h>
#include <UARTConnection.h>

UARTConnection uartMIDI;

void setup() {
    uartMIDI.begin(Serial1, /*RX=*/16, /*TX=*/17);
    midiHandler.addTransport(&uartMIDI);
    midiHandler.begin();
}
```

**Examples:** `UART-MIDI-Basic`, `P4-Dual-UART-MIDI`

---

## Architecture

```
INPUTS                              MIDIHandler              OUTPUTS

USB keyboard  --[USBConnection]------>  +--------------+
USB MIDI 2.0  --[USBMIDI2Connection]->  |              |
iPhone BLE    --[BLEConnection]------>  |  Event queue |--> getQueue()
macOS WiFi    --[RTPMIDIConnection]-->  |  (ring buf,  |
W5500 LAN     --[EthernetMIDIConn.]-->  |  thread-safe)|--> Active notes
Max/MSP OSC   --[OSCConnection]------>  |              |
DIN-5 serial  --[UARTConnection]----->  |  Chord       |--> Chord index
ESP32 radio   --[ESPNowConnection]--->  +------+-------+
                                               |
                                               v
                                        send* / sendMidiMessage()
                                        (first transport that accepts)
```

**Core 0** runs the USB Host task, BLE stack, and radio / network drivers (FreeRTOS tasks).
**Core 1** runs `midiHandler.task()` and your `loop()`. Every transport uses ring buffers and `portMUX` spinlocks for thread safety.

---

## Hardware compatibility

| Chip | USB Host | BLE | WiFi | Ethernet (native) | UART | ESP-NOW |
|------|:--------:|:---:|:----:|:-----------------:|:----:|:-------:|
| ESP32-S3 | yes | yes | yes | W5500 SPI | yes | yes |
| ESP32-S2 | yes | no | yes | W5500 SPI | yes | no |
| ESP32-P4 | yes | no | no | yes | yes (x5) | no |
| ESP32 (classic) | no | yes | yes | W5500 SPI | yes | yes |
| ESP32-C3 / C6 / H2 | no | yes | yes | no | yes | yes |

> W5500 SPI Ethernet works on any ESP32 through `EthernetMIDIConnection`. The **LilyGO T-Display-S3** (ESP32-S3 + 1.9" display) is the best all-round board for USB Host, BLE, WiFi, and a live MIDI dashboard.

---

## Examples

The [`examples/`](examples) folder ships runnable sketches, several with a photo and an `.mp4` demo in `examples/<name>/images/`.

| Example | Transport | What it shows |
|---------|-----------|---------------|
| `USB-Host-MIDI2` | USB Host MIDI 2.0 | Receive and decode raw UMP |
| `T-Display-S3-Piano-Flow` | USB Host MIDI 2.0 | Piano roll, chord with inversion, note duration |
| `T-Display-S3-BLE-Sender` | BLE | Send mode status + event log |
| `T-Display-S3-BLE-Receiver` | BLE | Receive mode + note log |
| `T-Display-S3-ESP-NOW-Jam` | ESP-NOW | Peer status + jam events |
| `RTP-MIDI-WiFi` | RTP-MIDI | Apple MIDI to macOS over WiFi |
| `Ethernet-MIDI` | Ethernet | Apple MIDI over W5500 / native MAC |
| `T-Display-S3-OSC` | OSC + WiFi | OSC to MIDI bridge with display |
| `UART-MIDI-Basic` | UART / DIN-5 | DIN-5 in and out |
| `P4-Dual-UART-MIDI` | UART / DIN-5 | Two hardware UARTs on the ESP32-P4 |

<p align="center">
  <img src="examples/RTP-MIDI-WiFi/images/RTP.jpeg" width="280" />&nbsp;
  <img src="examples/T-Display-S3-BLE-Receiver/images/BLE.jpeg" width="280" />
</p>
<p align="center"><em>RTP-MIDI / Apple MIDI to macOS · BLE MIDI from iPhone</em></p>

---

## Installation

**Arduino IDE:** Sketch > Include Library > Manage Libraries, search **ESP32_Host_MIDI**.

**PlatformIO:**

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board    = esp32-s3-devkitc-1
framework = arduino

lib_deps =
    sauloverissimo/ESP32_Host_MIDI
    # lathoub/Arduino-AppleMIDI-Library   ; RTP-MIDI + Ethernet MIDI
    # arduino-libraries/Ethernet          ; Ethernet MIDI
    # CNMAT/OSC                           ; OSC
```

**Board package:** `Tools > Boards Manager`, "esp32" by Espressif, version >= 3.0.0. USB Host requires arduino-esp32 >= 3.0 (TinyUSB MIDI).

| Transport | Required library |
|-----------|------------------|
| RTP-MIDI / Ethernet MIDI | `lathoub/Arduino-AppleMIDI-Library` |
| Ethernet MIDI | `arduino-libraries/Ethernet` |
| OSC | `CNMAT/OSC` |
| USB Host / BLE / ESP-NOW / UART | built into arduino-esp32 |

---

## API reference

```cpp
// Setup: every transport is explicit (v6.0+).
USBConnection usb;                 // #include <USBConnection.h>, <BLEConnection.h>, ...
BLEConnection ble;
ble.begin("My Device");            // user owns each transport's lifecycle
usb.begin();
midiHandler.addTransport(&usb);    // register each transport
midiHandler.addTransport(&ble);
midiHandler.begin();               // defaults
midiHandler.begin(cfg);            // or with a custom MIDIHandlerConfig
midiHandler.task();                // call every loop()

// Receive
const auto& q = midiHandler.getQueue();                          // event ring buffer
std::vector<std::string> n = midiHandler.getActiveNotesVector(); // ["C4","E4","G4"]
size_t count = midiHandler.getActiveNotesCount();
// SysEx: midiHandler.getSysExQueue(), setSysExCallback(cb), sendSysEx(data, len)

// Send (first transport that accepts the message wins)
midiHandler.sendNoteOn(ch, note, vel);        // ch: 1-16
midiHandler.sendNoteOff(ch, note, vel);
midiHandler.sendControlChange(ch, ctrl, val);
midiHandler.sendProgramChange(ch, prog);
midiHandler.sendPitchBend(ch, val);           // -8192..+8191, center = 0
```

**MIDIHandlerConfig:**

```cpp
MIDIHandlerConfig cfg;
cfg.maxEvents         = 20;    // queue capacity (1..100)
cfg.chordTimeWindow   = 0;     // ms grouping for chord detection (0 = legacy)
cfg.velocityThreshold = 0;     // ignore NoteOn below this velocity (0..127)
cfg.historyCapacity   = 0;     // PSRAM history buffer (0 = disabled)
cfg.maxSysExSize      = 512;   // bytes per SysEx (0 = disable SysEx)
cfg.maxSysExEvents    = 8;     // SysEx queue depth
midiHandler.begin(cfg);
```

**Custom transport:** subclass `MIDITransport`, implement `task()` and `isConnected()`, optionally `sendMidiMessage()`, and call the inherited `dispatchMidiData()` to inject received MIDI.

```cpp
class MyTransport : public MIDITransport {
    void task() override { /* read hardware, then dispatchMidiData(bytes, len) */ }
    bool isConnected() const override { return connected; }
    bool sendMidiMessage(const uint8_t* data, size_t len) override { /* ... */ return true; }
};
midiHandler.addTransport(&myTransport);
```

---

## License

MIT, see [LICENSE](LICENSE).

<p align="center">
  Built for musicians, makers, and researchers.<br/>
  Issues and contributions welcome at
  <a href="https://github.com/sauloverissimo/ESP32_Host_MIDI">github.com/sauloverissimo/ESP32_Host_MIDI</a>
</p>
