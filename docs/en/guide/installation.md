# Installation

The library supports three development environments: Arduino IDE, PlatformIO, and ESP-IDF (Arduino component).

---

## Requirements

!!! warning "Minimum ESP32 package version"
    USB Host and USB Device require **arduino-esp32 >= 3.0.0** (built-in TinyUSB MIDI).
    Check at: `Tools > Boards Manager > "esp32" by Espressif`

---

## Arduino IDE

### Step 1 -- Install the main library

```
Sketch > Include Library > Manage Libraries...
> Search: "ESP32_Host_MIDI"
> Install: ESP32_Host_MIDI by sauloverissimo
```

### Step 2 -- Install the ESP32 board package

```
Tools > Boards Manager
> Search: "esp32"
> Install: esp32 by Espressif Systems (version >= 3.0.0)
```

### Step 3 -- Install optional libraries (per transport)

Install only the ones you need:

| Transport | Library to install |
|-----------|-------------------|
| RTP-MIDI (WiFi) | `lathoub/Arduino-AppleMIDI-Library` (v3.x) |
| Ethernet MIDI | `lathoub/Arduino-AppleMIDI-Library` + `arduino-libraries/Ethernet` |
| OSC | `CNMAT/OSC` |
| Chord detection | `sauloverissimo/gingoduino` |
| USB Host / BLE / ESP-NOW | Already included in arduino-esp32 |
| USB Device | Already included in arduino-esp32 (TinyUSB) |
| UART / DIN-5 | No extra dependencies |

```
Sketch > Include Library > Manage Libraries...
> Search and install each one above
```

### Step 4 -- Set USB mode (for USB Host)

```
Tools > USB Mode > "USB Host"
```

!!! note "USB Device"
    For the USB Device transport (ESP32 presents itself as a MIDI interface), use:
    `Tools > USB Mode > "USB-OTG (TinyUSB)"`

---

## PlatformIO

Add to your `platformio.ini`:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board    = esp32-s3-devkitc-1
framework = arduino

lib_deps =
    sauloverissimo/ESP32_Host_MIDI
    ; Uncomment based on the transports you use:
    ; lathoub/Arduino-AppleMIDI-Library  ; RTP-MIDI + Ethernet MIDI
    ; arduino-libraries/Ethernet          ; Ethernet MIDI
    ; CNMAT/OSC                           ; OSC
    ; sauloverissimo/gingoduino           ; Chord detection

; For USB Host:
build_flags =
    -D ARDUINO_USB_MODE=0
    -D ARDUINO_USB_CDC_ON_BOOT=0
```

For USB Device:

```ini
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
```

---

## Manual Installation (symlink)

If you developed the library locally and want to test the examples in Arduino IDE:

```bash
# Create a symlink from your development folder to the Arduino libraries folder
ln -s /home/saulo/Libraries/ESP32_Host_MIDI /home/saulo/Arduino/libraries/ESP32_Host_MIDI
```

This lets you edit the source files directly without copying files.

---

## Verifying the Installation

After installing, open one of the examples:

```
File > Examples > ESP32_Host_MIDI > UART-MIDI-Basic
```

Compile (without uploading) to verify that all dependencies are resolved. If it compiles without errors, the installation is correct.

!!! tip "Minimal example -- no USB hardware needed"
    `UART-MIDI-Basic` is the simplest example for verifying the installation, since it does not require specific USB-OTG hardware.

---

## Chip Compatibility Table

```mermaid
graph TD
    subgraph CHIPS["ESP32 Chips"]
        S3["ESP32-S3\n✅ USB Host\n✅ BLE\n✅ USB Device\n✅ WiFi\n✅ ESP-NOW\n✅ UART"]
        S2["ESP32-S2\n✅ USB Host\n❌ BLE\n✅ USB Device\n✅ WiFi\n❌ ESP-NOW\n✅ UART"]
        P4["ESP32-P4\n✅ USB Host HS\n❌ BLE\n✅ USB Device\n❌ WiFi\n❌ ESP-NOW\n✅ UART x5\n✅ Ethernet MAC"]
        CLASSIC["ESP32 Classic\n❌ USB Host\n✅ BLE\n❌ USB Device\n✅ WiFi\n✅ ESP-NOW\n✅ UART"]
        C3["ESP32-C3/C6/H2\n❌ USB Host\n✅ BLE\n❌ USB Device\n✅ WiFi\n✅ ESP-NOW\n✅ UART"]
    end

    style S3 fill:#1B5E20,color:#fff,stroke:#2E7D32
    style S2 fill:#1565C0,color:#fff,stroke:#0D47A1
    style P4 fill:#4A148C,color:#fff,stroke:#6A1B9A
    style CLASSIC fill:#BF360C,color:#fff,stroke:#E64A19
    style C3 fill:#37474F,color:#fff,stroke:#546E7A
```

!!! tip "Recommended board"
    **LilyGO T-Display-S3** = ESP32-S3 + ST7789 1.9" display + LiPo battery. It is the most versatile board for ESP32_Host_MIDI: USB Host, BLE, WiFi, ESP-NOW, and display all in one.

---

## Next Steps

- [Getting Started ->](getting-started.md) -- first sketch up and running
- [Configuration ->](configuration.md) -- `MIDIHandlerConfig` options
