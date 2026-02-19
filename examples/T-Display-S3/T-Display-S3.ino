// Example: MIDI Controller Answer
// Displays the note names from the last MIDI chord on the ST7789 display of the T-Display S3.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include "ST7789_Handler.h"

// Delay for displaying initialization messages (ms)
static const unsigned long INIT_DISPLAY_DELAY = 500;

void setup() {
  Serial.begin(115200);

  display.init();
  display.print("Display OK...");
  delay(INIT_DISPLAY_DELAY);

  midiHandler.begin();
  display.print("MIDI Handler initialized...");

  // Disable history for this example (adjust as needed)
  midiHandler.enableHistory(0);
  display.print("USB & BLE MIDI Host initialized...");
  delay(INIT_DISPLAY_DELAY);
}

void loop() {
  midiHandler.task();
  std::vector<std::string> answer = midiHandler.getAnswer("noteName");
  display.print(answer);
  delayMicroseconds(10);
}
