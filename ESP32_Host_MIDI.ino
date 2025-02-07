#include <Arduino.h>
#include "ESP32_Host_MIDI.h"   // Biblioteca USB MIDI
#include "displayhandler.h"    // Handler para exibição no display LovyanGFX
#include "MIDI_handler.h"      // Módulo para interpretar as mensagens MIDI

// Aumenta o tempo de exibição para 1 segundo (1000 ms)
#define DISPLAY_TIMEOUT 1000

unsigned long lastMsgTime = 0;
bool msgDisplayed = false;

// Classe derivada que processa a mensagem MIDI usando o MIDI_handler
class MyESP32_Host_MIDI : public ESP32_Host_MIDI {
public:
  MyESP32_Host_MIDI(DisplayHandler* disp) : display(disp) {}

  // Sobrescreve onMidiMessage para interpretar e exibir a mensagem
  void onMidiMessage(const uint8_t *data, size_t length) override {
    // IMPORTANTE: Os dados USB MIDI vêm com um cabeçalho no byte 0 (ex: 0x09)
    // Para interpretar os dados reais, utiliza-se data+1.
    const uint8_t* midiData = data + 1;
    size_t midiLength = (length > 1) ? (length - 1) : 0;

    std::string rawStr            = MIDIHandler::getRawFormat(midiData, midiLength);
    std::string shortStr          = MIDIHandler::getShortFormat(midiData, midiLength);
    std::string noteNumStr        = MIDIHandler::getNoteNumberFormat(midiData, midiLength);
    std::string messageStr        = MIDIHandler::getMessageFormat(midiData, midiLength);
    std::string statusStr         = MIDIHandler::getMessageStatusFormat(midiData, midiLength);
    std::string noteSoundOctaveStr= MIDIHandler::getNoteSoundOctave(midiData, midiLength);
    
    // Cria a mensagem para exibição dividida em 5 linhas:
    // Linha 1: Raw
    // Linha 2: Short
    // Linha 3: Note#
    // Linha 4: Msg
    // Linha 5: Octave
    String displayMsg = "";
    displayMsg += "Raw: "      + String(rawStr.c_str())       + "\n";
    displayMsg += "Short: "    + String(shortStr.c_str())     + "\n";
    displayMsg += "Note#: "    + String(noteNumStr.c_str())   + "\n";
    displayMsg += "Msg: "      + String(messageStr.c_str())   + "\n";
    displayMsg += "Octave: "   + String(noteSoundOctaveStr.c_str());
    
    if (display) {
      display->printMidiMessage(displayMsg.c_str());
    }
    
    lastMsgTime = millis();
    msgDisplayed = true;
  }

private:
  DisplayHandler* display;
};

DisplayHandler display;
MyESP32_Host_MIDI usbMidi(&display);

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32_Host_MIDI com interpretação MIDI e display");
  
  display.init();
  usbMidi.begin();
}

void loop() {
  usbMidi.task();
  
  if (msgDisplayed && (millis() - lastMsgTime > DISPLAY_TIMEOUT)) {
    display.clear();
    msgDisplayed = false;
  }
  
  delay(10);
}
