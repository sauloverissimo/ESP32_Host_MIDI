# Troubleshooting

Solutions for the most common issues when using ESP32_Host_MIDI.

---

## USB Host

### USB keyboard is not detected

**Symptoms:** Serial Monitor shows nothing when pressing keys.

**Checklist:**

1. **Correct USB Mode?**
   ```
   Arduino IDE -> Tools -> USB Mode -> "USB Host"
   ```
   This option only appears for ESP32-S3, S2, or P4.

2. **Correct board selected?**
   Only ESP32-S3, S2, and P4 have USB-OTG. ESP32 Classic is not supported.

3. **Correct OTG cable?**
   - Use a **USB-OTG host** cable (micro-A or C to USB-A female)
   - Do not use a regular data cable -- the pinout is different
   - Make sure the cable supports data (not charge-only)

4. **Is the keyboard class-compliant?**
   Test by connecting to macOS -- if it is recognized without a driver, it is class-compliant.

5. **Sufficient power?**
   ```
   Serial.printf("USB voltage: %.2fV\n", /* measure on VBUS line */);
   ```
   USB MIDI devices need at least 100 mA at 5V.

---

### "USB Mode" does not appear in the Tools menu

**Cause:** The selected board does not support USB-OTG.

**Solution:** Switch to "LilyGo T-Display-S3", "ESP32-S3 Dev Module", or another S3/S2/P4 board.

---

### Upload fails after selecting "USB Host"

**Cause:** USB Host mode changes USB CDC behavior.

**Solutions:**

1. Press and hold **BOOT** + **RST** to enter bootloader mode
2. Or upload via UART (not OTG): connect through a USB-UART adapter
3. After uploading, disconnect/reconnect or press RST

---

## USB Device

### Windows: device shows up in Device Manager but not in the DAW

**Symptoms:** The ESP32 appears as "TinyUSB MIDI" in Device Manager and Serial works, but DAWs and MIDI software (MIDI-OX, etc.) do not list the MIDI port.

**Cause:** When CDC (Serial over USB) and MIDI are both active, Windows creates a composite device. The CDC driver (`usbser.sys`) loads correctly, but the MIDI driver (`usbaudio.sys`) may not be automatically associated with the MIDI interface.

**Solutions:**

1. **Quick test -- disable CDC:**
   ```
   Arduino IDE -> Tools -> USB CDC on Boot -> Disabled
   ```
   Re-flash (you may need to hold BOOT + RST to enter bootloader).
   If the MIDI port shows up in the DAW, this confirms it is a composite device issue.

2. **For development with Serial + MIDI:**
   - Use UART on separate pins for serial debug (e.g. GPIO43/TX, GPIO44/RX with a USB-UART adapter)
   - Keep native USB for MIDI only

3. **Windows fix (no re-flash):**
   - Device Manager -> right-click "TinyUSB MIDI" -> Update Driver -> Browse -> "Let me pick from a list" -> select "USB Audio Device"
   - If it does not appear in the list: uninstall the device, disconnect USB, reconnect

4. **Windows driver cache:**
   - After switching from CDC enabled to disabled (or vice-versa), uninstall the device in Device Manager before reconnecting
   - Windows caches drivers by VID/PID and may keep the old configuration

!!! note "macOS and Linux"
    This issue is exclusive to Windows. macOS and Linux load the class-compliant driver correctly even with composite devices (CDC + MIDI).

---

## BLE MIDI

### BLE device does not appear on iOS/macOS

**Symptoms:** The configured name does not appear in the Bluetooth device list.

**Checklist:**

1. **Bluetooth enabled in sdkconfig?**
   ```
   Arduino IDE -> Tools -> Partition Scheme -> "Default 4MB with spiffs"
   ```
   Make sure you are using a partition scheme that includes BLE (>1.5 MB app).

2. **Does the chip support BLE?**
   ESP32-S2 and ESP32-P4 **do not have Bluetooth**. Use ESP32, S3, C3, or C6.

3. **Check the macro:**
   ```cpp
   #if ESP32_HOST_MIDI_HAS_BLE
       Serial.println("BLE available");
   #else
       Serial.println("BLE NOT available on this chip");
   #endif
   ```

4. **Advertising started?**
   ```cpp
   midiHandler.begin();
   // BLE starts advertising automatically after begin()
   delay(1000);
   Serial.println("BLE advertising...");
   ```

5. **UUID conflict?**
   If another BLE device with the same name is nearby, it may conflict.
   Change `cfg.bleName` to a unique name.

---

### BLE disconnects frequently

**Possible causes:**

- Distance > 15 m (walls reduce range)
- Interference from other 2.4 GHz devices (WiFi, microwaves)
- Insufficient power (voltage drop during BLE peaks)

**Solutions:**

- Add a 100uF capacitor on the ESP32 power supply
- Reduce WiFi power if using WiFi + BLE simultaneously
- BLE restarts advertising automatically -- no code changes needed

---

## RTP-MIDI (WiFi)

### ESP32 does not appear in Audio MIDI Setup

**Checklist:**

1. **ESP32 and Mac on the same WiFi network?**
   ```cpp
   Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
   ```
   Confirm the IP is on the same subnet (e.g. 192.168.1.x / 192.168.1.x).

2. **macOS firewall blocking mDNS?**
   Check in: System Settings -> Network -> Firewall -> Allow incoming connections.

3. **AppleMIDI-Library v3.x installed?**
   ```
   Manage Libraries -> "AppleMIDI" -> version >= 3.0.0
   ```

4. **WiFi connected BEFORE rtpMIDI.begin()?**
   ```cpp
   WiFi.begin(ssid, pass);
   while (WiFi.status() != WL_CONNECTED) delay(500);  // <- Wait!
   rtpMIDI.begin("My ESP32");  // <- Only after
   ```

5. **mDNS active?**
   The AppleMIDI library uses MDNS. Check that your router does not block mDNS (multicast).

---

### RTP-MIDI connects but no sound in the DAW

**Cause:** The network session needs to be enabled manually in Audio MIDI Setup.

**Solution:**
1. Audio MIDI Setup -> Network -> select the session -> click **Connect**
2. In the DAW: check that the MIDI port "My ESP32" is enabled as input

---

## UART / DIN-5

### No messages received via DIN-5

**Checklist:**

1. **Correct RX/TX pins?**
   ```cpp
   uartMIDI.begin(Serial1, /*RX=*/16, /*TX=*/17);
   // Make sure the MIDI cable is plugged into MIDI IN on the instrument
   // and connected to the ESP32 RX pin
   ```

2. **Optocoupler wired correctly?**
   Test without the opto: connect the opto output pin directly to 3.3V through a 1k resistor.
   If RX detects HIGH, the pin is working.

3. **Correct baud rate?**
   MIDI uses **31250 bps** -- do not confuse with 31200, 38400, or other similar values.

4. **Instrument sending MIDI?**
   Test with another device (computer + MIDI interface) to confirm the instrument is sending MIDI.

5. **GPIO 0 not being used as RX?**
   GPIO 0 is the boot button -- avoid using it for UART.

---

## PSRAM

### PSRAM not detected / enableHistory() fails

**Checklist:**

1. **PSRAM enabled in Arduino IDE?**
   ```
   Tools -> PSRAM -> "OPI PSRAM" (T-Display-S3)
                or "Quad PSRAM" (other S3 boards)
   ```

2. **Check availability:**
   ```cpp
   Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
   if (ESP.getPsramSize() == 0) {
       Serial.println("PSRAM not detected!");
   }
   ```

3. **Correct macro:**
   ```cpp
   #if ESP32_HOST_MIDI_HAS_PSRAM
       Serial.println("PSRAM available");
   #else
       Serial.println("PSRAM NOT available");
   #endif
   ```

---

## Compilation

### "error: 'map' was not declared"

**Cause:** Missing `#include <map>` in a header.

**Solution:** This bug was fixed in recent versions. Update the library:
```
Manage Libraries -> ESP32_Host_MIDI -> Update
```

---

### "addTransport() limit exceeded"

**Cause:** You registered more than 4 external transports via `addTransport()`.

**Solution:** The limit is 4 external transports. Built-in USB, BLE, and ESP-NOW do not count.
Combine transports or reduce the number of external transports.

---

## Debug -- Raw MIDI Callback

To inspect raw bytes before parsing:

```cpp
void onRaw(const uint8_t* raw, size_t len, const uint8_t* midi3) {
    Serial.printf("RAW [%d bytes]: ", (int)len);
    for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", raw[i]);
    }
    Serial.printf("| MIDI: %02X %02X %02X\n",
        midi3[0], midi3[1], midi3[2]);
}

void setup() {
    midiHandler.setRawMidiCallback(onRaw);
    midiHandler.begin();
}
```

---

## Filing an Issue

If the problem persists after all checks:

1. Note the library version (`library.properties`)
2. Note the chip and board
3. Copy the Serial Monitor output (with the raw debug callback)
4. Open an issue at [github.com/sauloverissimo/ESP32_Host_MIDI/issues](https://github.com/sauloverissimo/ESP32_Host_MIDI/issues)

---

## Next Steps

- [Supported Hardware ->](hardware.md) -- check chip compatibility
- [API ->](../api/reference.md) -- verify correct signatures
- [Getting Started ->](../guia/primeiros-passos.md) -- go back to basics
