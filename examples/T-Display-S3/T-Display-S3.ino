
// Exemplo: MIDI Controller Answer
// Exibe as notas do último bloco MIDI no display ST7789 do T-Display S3.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include "ST7789_Handler.h"

void setup() {
  Serial.begin(115200);

  display.init();
  display.print("Display OK...");
  delay(500);

  midiHandler.begin();
  display.print("Interpretador MIDI inicializado...");

  // Desativa o histórico para este exemplo (ajuste conforme necessidade)
  midiHandler.enableHistory(0);
  display.print("Host USB & BLE MIDI Inicializado...");
  delay(500);
}

void loop() {
  midiHandler.task();
  std::vector<std::string> resposta = midiHandler.getAnswer("som");
  display.print(resposta);
  delayMicroseconds(10);
}
