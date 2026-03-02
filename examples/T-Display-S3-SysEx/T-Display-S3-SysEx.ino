// Example: SysEx Monitor
// Displays received SysEx messages and MIDI events on the T-Display-S3.
// BTN1 (GPIO0)  — send Identity Request (F0 7E 7F 06 01 F7)
// BTN2 (GPIO14) — clear SysEx queue
//
// The SysEx queue is separate from the normal event queue.
// Existing code using getQueue() is unaffected.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include "ST7789_Handler.h"
#include "mapping.h"

static const unsigned long INIT_DISPLAY_DELAY = 500;
static const int MAX_DISPLAY_LINES = 11;

// Format a SysEx message as hex string (e.g. "F0 7E 7F 06 02 ... F7")
static String formatSysEx(const std::vector<uint8_t>& data, int maxBytes = 18) {
  String s;
  int len = data.size();
  int show = (len <= maxBytes) ? len : maxBytes;
  for (int i = 0; i < show; i++) {
    if (i > 0) s += ' ';
    char hex[4];
    sprintf(hex, "%02X", data[i]);
    s += hex;
  }
  if (len > maxBytes) {
    s += " ...(";
    s += String(len);
    s += "B)";
  }
  return s;
}

// Identify common SysEx message types
static String identifySysEx(const std::vector<uint8_t>& data) {
  if (data.size() < 4) return "Unknown";
  if (data[1] == 0x7E) {
    // Universal Non-Real-Time
    if (data.size() >= 6 && data[3] == 0x06) {
      if (data[4] == 0x01) return "Identity Request";
      if (data[4] == 0x02) return "Identity Reply";
    }
    return "Universal Non-RT";
  }
  if (data[1] == 0x7F) return "Universal Real-Time";
  // Manufacturer-specific
  if (data[1] == 0x00 && data.size() >= 4) {
    char mfr[12];
    sprintf(mfr, "Mfr %02X%02X%02X", data[1], data[2], data[3]);
    return String(mfr);
  }
  char mfr[8];
  sprintf(mfr, "Mfr %02X", data[1]);
  return String(mfr);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BUTTON_1, INPUT_PULLUP);
  pinMode(PIN_BUTTON_2, INPUT_PULLUP);

  display.init();
  display.print("Display OK...");
  delay(INIT_DISPLAY_DELAY);

  MIDIHandlerConfig config;
  config.maxSysExSize = 256;
  config.maxSysExEvents = 8;
  midiHandler.begin(config);

  display.print("SysEx Monitor ready.\nConnect USB MIDI device.\nBTN1=Identity Request\nBTN2=Clear");
  delay(INIT_DISPLAY_DELAY * 2);
}

void loop() {
  midiHandler.task();

  // BTN1: send MIDI Identity Request
  static bool btn1Held = false;
  if (digitalRead(PIN_BUTTON_1) == LOW) {
    if (!btn1Held) {
      btn1Held = true;
      const uint8_t identityReq[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
      midiHandler.sendSysEx(identityReq, sizeof(identityReq));
      Serial.println("Sent Identity Request");
    }
  } else {
    btn1Held = false;
  }

  // BTN2: clear SysEx queue
  if (digitalRead(PIN_BUTTON_2) == LOW) {
    midiHandler.clearSysExQueue();
    midiHandler.clearQueue();
    delay(200);
  }

  // Build display output
  const auto& sysexQueue = midiHandler.getSysExQueue();
  const auto& eventQueue = midiHandler.getQueue();
  String log;

  // Header line
  log += "SysEx:" + String(sysexQueue.size()) + " MIDI:" + String(eventQueue.size());
  log += " Notes:" + String(midiHandler.getActiveNotesCount()) + "\n";

  if (sysexQueue.empty() && eventQueue.empty()) {
    log += "[Waiting for MIDI data...]\n";
    log += "BTN1=Send Identity Request\n";
  } else {
    // Show SysEx messages (most recent first)
    int lines = 0;
    for (auto it = sysexQueue.rbegin(); it != sysexQueue.rend() && lines < 5; ++it, ++lines) {
      log += "#" + String(it->index) + " " + identifySysEx(it->data) + "\n";
      log += " " + formatSysEx(it->data) + "\n";
    }

    // Separator if we have both types
    if (!sysexQueue.empty() && !eventQueue.empty()) {
      log += "---\n";
    }

    // Show recent MIDI events (fill remaining lines)
    int remaining = MAX_DISPLAY_LINES - (lines * 2) - 1;
    if (!sysexQueue.empty() && !eventQueue.empty()) remaining--;
    int count = 0;
    for (auto it = eventQueue.rbegin(); it != eventQueue.rend() && count < remaining; ++it, ++count) {
      char line[80];
      sprintf(line, "%s ch%d %s v%d",
              it->status.c_str(), it->channel,
              it->noteOctave.c_str(), it->velocity);
      log += String(line) + "\n";
    }
  }

  display.print(log.c_str());
  delayMicroseconds(1);
}
