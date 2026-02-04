// Exemplo: MIDI Data Queue
// Exibe a fila de eventos MIDI e notas ativas no display ST7789 do T-Display S3.
// Útil para debug e visualização detalhada dos eventos MIDI recebidos.

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include "ST7789_Handler.h"

// Tempo de espera para exibir mensagens de inicialização (ms)
static const unsigned long INIT_DISPLAY_DELAY = 500;
// Número máximo de eventos exibidos na tela
static const int MAX_DISPLAY_EVENTS = 12;

void setup() {
  Serial.begin(115200);

  display.init();
  display.print("Display OK...");
  delay(INIT_DISPLAY_DELAY);

  midiHandler.begin();
  display.print("Interpretador MIDI inicializado...");

  midiHandler.enableHistory(0);
  display.print("Host USB & BLE MIDI Inicializado...");
  delay(INIT_DISPLAY_DELAY);
}

void loop() {
  midiHandler.task();

  const auto& queue = midiHandler.getQueue();
  String log;

  if (queue.empty()) {
    log = "[Press any key to start...]\n[Aperte uma tecla para iniciar...]";
  } else {
    std::string active = midiHandler.getActiveNotes();
    size_t NotesCount = midiHandler.getActiveNotesCount();
    log += "[" + String(NotesCount) + "] " + String(active.c_str()) + "\n";

    int count = 0;
    for (auto it = queue.rbegin(); it != queue.rend() && count < MAX_DISPLAY_EVENTS; ++it, ++count) {
      char line[200];
      sprintf(line, "%d;%d;%lu;%lu;%d;%s;%d;%s;%s;%d;%d;%d",
              it->index, it->msgIndex, it->tempo, it->delay, it->canal,
              it->mensagem.c_str(), it->nota, it->som.c_str(), it->oitava.c_str(),
              it->velocidade, it->flushOff, it->blockIndex);
      log += String(line) + "\n";
    }
  }

  display.print(log.c_str());
  delayMicroseconds(1);
}
