// ESP32_Host_MIDI / T-Display-S3-Queue
// USB Host + BLE MIDI: show the event queue and active notes on the ST7789 display.
//
// Requires: LovyanGFX.
// Arduino IDE: Board T-Display-S3 (ESP32-S3) | Partition Scheme: Huge App (3MB) | Serial 115200

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>   // v6.0: transports are no longer auto-included
#include <BLEConnection.h>
#include "ST7789_Handler.h"
#include "mapping.h"

USBConnection usbHost;       // v6.0: explicit USB Host transport
BLEConnection bleHost;       // v6.0: explicit BLE peripheral

// Delay for displaying initialization messages (ms)
static const unsigned long INIT_DISPLAY_DELAY = 500;
// Maximum number of events displayed on screen
static const int MAX_DISPLAY_EVENTS = 12;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BUTTON_2, INPUT_PULLUP);

  display.init();
  display.print("Display OK...");
  delay(INIT_DISPLAY_DELAY);

  midiHandler.addTransport(&usbHost);  // v6.0: explicit
  midiHandler.addTransport(&bleHost);
  usbHost.begin();
  bleHost.begin("ESP32 Queue");
  midiHandler.begin();
  display.print("MIDI Handler initialized...");

  midiHandler.enableHistory(0);
  display.print("USB & BLE MIDI Host initialized...");
  delay(INIT_DISPLAY_DELAY);
}

void loop() {
  midiHandler.task();

  // Button 2: clear the queue and reset MIDI state
  if (digitalRead(PIN_BUTTON_2) == LOW) {
    midiHandler.clearQueue();
    delay(200);
  }

  const auto& queue = midiHandler.getQueue();
  String log;

  if (queue.empty()) {
    log = "[Press any key to start...]";
  } else {
    std::string active = midiHandler.getActiveNotes();
    size_t NotesCount = midiHandler.getActiveNotesCount();
    log += "[" + String(NotesCount) + "] " + String(active.c_str()) + "\n";

    int count = 0;
    for (auto it = queue.rbegin(); it != queue.rend() && count < MAX_DISPLAY_EVENTS; ++it, ++count) {
      char line[200];
      char octBuf[8];
      sprintf(line, "%d;%d;%lu;%lu;%d;%s;%d;%s;%s;%d;%d",
              it->index, it->msgIndex, it->timestamp, it->delay, it->channel0,
              MIDIHandler::statusName(it->statusCode), it->noteNumber,
              MIDIHandler::noteName(it->noteNumber),
              MIDIHandler::noteWithOctave(it->noteNumber, octBuf, sizeof(octBuf)),
              it->velocity7, it->chordIndex);
      log += String(line) + "\n";
    }
  }

  display.print(log.c_str());
  delayMicroseconds(1);
}
