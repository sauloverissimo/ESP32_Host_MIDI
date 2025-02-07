#include <Arduino.h>
#include "ESP32_Host_MIDI.h"  // Inclui a biblioteca ESP32_Host_MIDI

// Cria uma instância da classe para gerenciar o USB MIDI
ESP32_Host_MIDI usbMidi;

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32_Host_MIDI");
  
  // Inicializa o USB Host e registra o cliente
  usbMidi.begin();
}

void loop() {
  // Processa eventos USB e as submissões de transferência
  usbMidi.task();
  
  // Outras tarefas podem ser adicionadas aqui, por exemplo,
  // atualização de display ou processamento adicional dos dados MIDI.
}
