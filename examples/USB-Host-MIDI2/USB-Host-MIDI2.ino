// USB-Host-MIDI2 — ESP32_Host_MIDI USB Host MIDI 2.0 (UMP) example
//
// Receives Universal MIDI Packets from a connected USB MIDI 2.0 device and
// prints each packet to Serial, decoding MIDI 2.0 Channel Voice and MIDI 1.0
// Channel Voice. Mirrors the TinyUSB host example examples/host/midi2_host.
//
// USBMIDI2Connection scans the device for Alt 0 (MIDI 1.0) and Alt 1
// (MIDI 2.0/UMP), preferring MIDI 2.0. Raw 32-bit UMP words are delivered
// one whole packet at a time via the UMP callback (no CIN, no conversion).
//
// Connect a USB MIDI 2.0 device to the host port and open Serial Monitor at
// 115200. A MIDI 1.0 device is received too, via the MIDI bytes callback.
//
// Arduino IDE setup:
//   Tools > Board    -> ESP32-S3 / ESP32-S2 / ESP32-P4 (with USB host wiring)
//   Tools > USB Mode -> Hardware CDC and JTAG
//   Serial Monitor   -> 115200 baud
//
// The native ESP-IDF USB Host driver takes over the OTG PHY at begin(); the
// log goes out over UART, so the USB connector is free for the device.

#include <Arduino.h>
#include <USBMIDI2Connection.h>

USBMIDI2Connection usb;

// Decode one UMP packet (1..4 words). Channel Voice is decoded; everything
// else is printed as hex. Mirrors print_ump() in the TinyUSB midi2_host example.
static void print_ump(const uint32_t* words, uint8_t wc) {
  uint8_t mt    = (words[0] >> 28) & 0x0F;
  uint8_t group = (words[0] >> 24) & 0x0F;

  if (mt == 0x4 && wc >= 2) {
    // MIDI 2.0 Channel Voice
    uint8_t status  = (words[0] >> 20) & 0x0F;
    uint8_t channel = (words[0] >> 16) & 0x0F;
    uint8_t data1   = (words[0] >> 8)  & 0x7F;
    switch (status) {
      case 0x9:
        Serial.printf("[g%u ch%u] M2 NoteOn  n=%u vel=%04X attr=%04X\n",
                      group, channel, data1,
                      (unsigned)((words[1] >> 16) & 0xFFFF),
                      (unsigned)(words[1] & 0xFFFF));
        break;
      case 0x8:
        Serial.printf("[g%u ch%u] M2 NoteOff n=%u vel=%04X\n",
                      group, channel, data1,
                      (unsigned)((words[1] >> 16) & 0xFFFF));
        break;
      case 0xB:
        Serial.printf("[g%u ch%u] M2 CC#%u = %08lX\n",
                      group, channel, data1, (unsigned long)words[1]);
        break;
      case 0xC:
        Serial.printf("[g%u ch%u] M2 ProgChg %u\n",
                      group, channel, (unsigned)((words[1] >> 24) & 0x7F));
        break;
      case 0xD:
        Serial.printf("[g%u ch%u] M2 ChanPress %08lX\n",
                      group, channel, (unsigned long)words[1]);
        break;
      case 0xE:
        Serial.printf("[g%u ch%u] M2 PitchBend %08lX\n",
                      group, channel, (unsigned long)words[1]);
        break;
      default:
        Serial.printf("[g%u ch%u] M2 status=0x%X w0=%08lX w1=%08lX\n",
                      group, channel, status,
                      (unsigned long)words[0], (unsigned long)words[1]);
        break;
    }
  } else if (mt == 0x2 && wc == 1) {
    // MIDI 1.0 Channel Voice in UMP
    uint8_t status = (words[0] >> 16) & 0xFF;
    uint8_t data1  = (words[0] >> 8)  & 0x7F;
    uint8_t data2  = words[0] & 0x7F;
    Serial.printf("[g%u] M1 %02X %02X %02X\n", group, status, data1, data2);
  } else {
    Serial.printf("UMP MT=0x%X wc=%u w0=%08lX\n",
                  mt, wc, (unsigned long)words[0]);
  }
}

// UMP callback: ESP32_Host_MIDI delivers one whole UMP packet per call, so
// there is no need to walk a buffer with a per-message word count.
static void onUMP(void* /*ctx*/, const uint32_t* words, uint8_t count) {
  print_ump(words, count);
}

// MIDI 1.0 bytes callback: fires when the device enumerates as MIDI 1.0 (Alt 0).
static void onMidi1(void* /*ctx*/, const uint8_t* data, size_t len) {
  Serial.print("M1 bytes:");
  for (size_t i = 0; i < len; i++) Serial.printf(" %02X", data[i]);
  Serial.println();
}

static void onConnect(void* /*ctx*/)    { Serial.println("USB MIDI device mounted"); }
static void onDisconnect(void* /*ctx*/) { Serial.println("USB MIDI device unmounted"); }

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nESP32_Host_MIDI - USB Host MIDI 2.0 (UMP) example");

  usb.setUMPCallback(onUMP, nullptr);
  usb.setMidiCallback(onMidi1, nullptr);
  usb.setConnectionCallbacks(onConnect, onDisconnect, nullptr);

  if (!usb.begin()) {
    Serial.print("USB host init failed: ");
    Serial.println(usb.getLastError());
  } else {
    Serial.println("USB host started. Plug a USB MIDI device into the host port.");
  }
}

void loop() {
  usb.task();
}
