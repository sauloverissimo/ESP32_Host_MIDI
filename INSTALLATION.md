# Installation Guide

## Requirements

- **Arduino IDE** 2.x (or Arduino CLI / PlatformIO)
- **ESP32 board package** (arduino-esp32) **v3.0.0 or later** -- required for USB Host / TinyUSB
- An **ESP32-S3** board (recommended) or ESP32-S2 / ESP32-P4

> The ESP32-S3 is the most versatile chip for this library: USB Host, BLE, WiFi, ESP-NOW, and USB Device all in one.

---

## Arduino IDE -- Step by Step

### 1. Install the ESP32 board package

```
Tools > Board > Boards Manager...
  Search: "esp32"
  Install: "esp32 by Espressif Systems" (version 3.0.0 or later)
```

### 2. Install ESP32\_Host\_MIDI

**From Library Manager (stable release):**

```
Sketch > Include Library > Manage Libraries...
  Search: "ESP32_Host_MIDI"
  Install: ESP32_Host_MIDI by sauloverissimo
```

**From a specific branch (e.g. experimental features):**

1. Go to the repository: https://github.com/sauloverissimo/ESP32_Host_MIDI
2. Switch to the branch you want (e.g. `feature/multi-usb-device`)
3. Click **Code > Download ZIP**
4. In Arduino IDE: `Sketch > Include Library > Add .ZIP Library...` and select the downloaded file

> **Important:** If you already have a Library Manager version installed, **remove it first** to avoid conflicts. You can find it in your Arduino libraries folder (typically `~/Arduino/libraries/ESP32_Host_MIDI/` on Linux/Mac or `Documents\Arduino\libraries\ESP32_Host_MIDI\` on Windows).

### 3. Select the board

```
Tools > Board > esp32 > ESP32S3 Dev Module
```

If you have a specific board (e.g. LilyGO T-Display-S3), select it instead. For generic ESP32-S3 boards, `ESP32S3 Dev Module` works.

### 4. Configure USB mode

For **USB Host** (connecting USB MIDI devices to the ESP32):

```
Tools > USB Mode > "USB Host"
```

For **USB Device** (ESP32 appears as a MIDI device to a computer):

```
Tools > USB Mode > "USB-OTG (TinyUSB)"
```

> USB Host and USB Device cannot be active at the same time -- pick one.

### 5. Other board settings (recommended)

```
Tools > PSRAM > "OPI PSRAM" (if your board has PSRAM)
Tools > Flash Size > "16MB" or whatever your board has
Tools > Partition Scheme > "Huge APP (3MB No OTA/1MB SPIFFS)" (for large sketches)
Tools > Upload Speed > 921600
```

### 6. Verify the installation

Open a minimal example that does not require extra hardware:

```
File > Examples > ESP32_Host_MIDI > UART-MIDI-Basic
```

Click **Verify** (compile without uploading). If it compiles without errors, the installation is correct.

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

; For USB Host:
build_flags =
    -D ARDUINO_USB_MODE=0
    -D ARDUINO_USB_CDC_ON_BOOT=0
```

For USB Device mode, change the build flags:

```ini
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
```

To use a specific branch instead of the release:

```ini
lib_deps =
    https://github.com/sauloverissimo/ESP32_Host_MIDI.git#feature/multi-usb-device
```

---

## Optional Dependencies

Install only the libraries you actually need, based on which transports you plan to use:

| Transport | Library to install |
|-----------|-------------------|
| USB Host | Built into arduino-esp32 (nothing extra) |
| BLE MIDI | Built into arduino-esp32 (nothing extra) |
| USB Device | Built into arduino-esp32 / TinyUSB (nothing extra) |
| ESP-NOW | Built into arduino-esp32 (nothing extra) |
| UART / DIN-5 | No extra dependencies |
| MIDI 2.0 UDP | No extra dependencies |
| RTP-MIDI (WiFi) | `lathoub/Arduino-AppleMIDI-Library` (v3.x) |
| Ethernet MIDI | `lathoub/Arduino-AppleMIDI-Library` + `arduino-libraries/Ethernet` |
| OSC | `CNMAT/OSC` |
| Chord detection | `sauloverissimo/gingoduino` |

---

## Experimental: Multi-USB-Device (Hub Support)

The `feature/multi-usb-device` branch adds support for connecting **multiple USB MIDI devices** through a USB hub.

### What you need

- **ESP32-S3** board (2-3 devices at Full-Speed) or **ESP32-P4** (more devices at High-Speed)
- A **powered USB hub** (passive hubs may not supply enough current for multiple devices)
- USB MIDI devices to connect

### How to install

Since this is an experimental branch, install from source:

1. **Remove** any existing ESP32_Host_MIDI from your Arduino libraries folder
2. Download the branch: https://github.com/sauloverissimo/ESP32_Host_MIDI/archive/refs/heads/feature/multi-usb-device.zip
3. In Arduino IDE: `Sketch > Include Library > Add .ZIP Library...`
4. Select the downloaded ZIP

Or clone directly:

```bash
cd ~/Arduino/libraries
git clone -b feature/multi-usb-device https://github.com/sauloverissimo/ESP32_Host_MIDI.git
```

### Board settings

```
Tools > Board        > ESP32S3 Dev Module
Tools > USB Mode     > USB Host
Tools > PSRAM        > OPI PSRAM (if available)
```

### Example sketch

Open: `File > Examples > ESP32_Host_MIDI > USB-Hub-Multi-Device`

This example will:
- Initialize the USB Host stack with hub support
- Detect USB MIDI devices as they connect/disconnect
- Print incoming MIDI messages with device identification
- Report connected device count every 5 seconds

### Limitations

- Maximum 4 simultaneous USB MIDI devices
- ESP32-S3 at Full-Speed (12 Mbps): 2-3 devices recommended
- USB Host and USB Device cannot coexist
- This is experimental -- feedback and bug reports are welcome

---

## Board Compatibility

| Chip | USB Host | BLE | USB Device | WiFi | ESP-NOW | UART |
|------|----------|-----|------------|------|---------|------|
| **ESP32-S3** | Yes | Yes | Yes | Yes | Yes | Yes |
| ESP32-S2 | Yes | No | Yes | Yes | No | Yes |
| ESP32-P4 | Yes (HS) | No | Yes | No | No | Yes (x5) |
| ESP32 (classic) | No | Yes | No | Yes | Yes | Yes |
| ESP32-C3/C6/H2 | No | Yes | No | Yes | Yes | Yes |

---

## Troubleshooting

**"Library not found" or include errors after installing from ZIP:**
Make sure the extracted folder is named `ESP32_Host_MIDI` (not `ESP32_Host_MIDI-feature-multi-usb-device` or similar). Arduino IDE requires the folder name to match the library name.

**"Multiple libraries found" warning:**
You have more than one copy installed. Check both the Library Manager location and your manual libraries folder. Remove the duplicate.

**USB Host not working / device not detected:**
- Confirm `Tools > USB Mode` is set to `USB Host`
- Check your USB cable supports data (not charge-only)
- For ESP32-S3 boards without a dedicated Host port, you may need a USB-OTG adapter

**Compilation fails with TinyUSB errors:**
- Make sure your ESP32 board package is v3.0.0 or later
- Try `Tools > Board > Boards Manager`, search "esp32", and update if needed

**Hub not detecting multiple devices:**
- Use a **powered** USB hub
- Try connecting devices one at a time
- Check Serial Monitor at 115200 baud for debug messages
