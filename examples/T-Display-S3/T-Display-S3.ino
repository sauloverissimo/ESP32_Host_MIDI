
// MIDI Controller Answer

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

  // Desativa o histórico para este teste (ou ajuste conforme sua necessidade)
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



// MIDI Data Queue

// #include <Arduino.h>
// #include <ESP32_Host_MIDI.h>
// #include "ST7789_Handler.h"

// void setup() {
// Serial.begin(115200);

//   display.init();
//   display.print("Display OK...");
//   delay(500);

//   midiHandler.begin();
//   display.print("Interpretador MIDI inicializado...");

//   midiHandler.enableHistory(0);
//   display.print("Host USB & BLE MIDI Inicializado...");
//   delay(500);

// }

// void loop() {
//   midiHandler.task();

//   static String ultimaMsg;
//   const auto& queue = midiHandler.getQueue();
//   String log;
//   if(queue.empty()){
//     log = "[Press any key to start...]\n[Aperte uma tecla para iniciar...]";
//   }
//   else {
//     std::string active = midiHandler.getActiveNotes();
//     size_t NotesCount = midiHandler.getActiveNotesCount();
//     log += "[" + String(NotesCount)+ "] " + String(active.c_str()) + "\n";
//     int count = 0;
//     for(auto it = queue.rbegin(); it != queue.rend() && count < 12; ++it, ++count){
//       char line[200];
//       sprintf(line, "%d;%d;%lu;%lu;%d;%s;%d;%s;%s;%d;%d;%d",
//               it->index, it->msgIndex, it->tempo, it->delay, it->canal,
//               it->mensagem.c_str(), it->nota, it->som.c_str(), it->oitava.c_str(),
//               it->velocidade, it->flushOff, it->blockIndex);
//       log += String(line) + "\n";
//     }
//   }
//   display.print(log.c_str());
//   delayMicroseconds(1);
// }



