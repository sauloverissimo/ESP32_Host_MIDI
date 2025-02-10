// ESP32_Host_MIDI Library
#include <Arduino.h>
#include "ESP32_Host_MIDI.h"         
#include "ESP32_BLE.h"
#include "MIDI_handler.h"

// T-Display S3 Modules - Examples Files 
#include "displayhandler.h"          // Handler para exibição no display LovyanGFX
#include "ESP32_PCM5102A_MIDI.h"     // Biblioteca para tocar notas via PCM5102A DAC
#include <math.h>

#define DISPLAY_TIMEOUT 1000  // 1 segundo de exibição

unsigned long lastMsgTime = 0;
bool msgDisplayed = false;

// Instância para geração de som
ESP32_PCM5102A_MIDI audio;

// Função auxiliar para converter nota MIDI para frequência
float midiNoteToFrequency(uint8_t note) {
  return 440.0 * pow(2, ((int)note - 69) / 12.0);
}

// Declaração da função de callback para mensagens BLE MIDI
void onBLEMessage(const uint8_t *data, size_t length);

// Instância global do BLE MIDI
ESP32_BLE bleMIDI;

// Classe derivada que processa a mensagem MIDI USB utilizando o MIDI_Handler
class MyESP32_Host_MIDI : public ESP32_Host_MIDI {
public:
  MyESP32_Host_MIDI(DisplayHandler* disp) : display(disp) {}

  // Sobrescreve onMidiMessage para interpretar, exibir e tocar a nota
  void onMidiMessage(const uint8_t *data, size_t length) override {
    // Remove o cabeçalho USB MIDI (primeiro byte)
    const uint8_t* midiData = data + 1;
    size_t midiLength = (length > 1) ? (length - 1) : 0;

    // Geração dos formatos para exibição via MIDI_Handler
    std::string rawStr             = MIDIHandler::getUsbMidiFormat(midiData, midiLength);
    std::string shortStr           = MIDIHandler::getShortFormat(midiData, midiLength);
    std::string noteNumStr         = MIDIHandler::getNoteNumberFormat(midiData, midiLength);
    std::string messageStr         = MIDIHandler::getMessageFormat(midiData, midiLength);
    std::string statusStr          = MIDIHandler::getMessageStatusFormat(midiData, midiLength);
    std::string noteSoundOctaveStr = MIDIHandler::getNoteSoundOctave(midiData, midiLength);

    // Calcula frequência e amplitude (se aplicável)
    float frequency = (midiLength >= 2) ? midiNoteToFrequency(midiData[1]) : 0.0;
    int amplitude = (midiLength >= 3) ? (int)(midiData[2] / 127.0 * 32767) : 0;

    // Cria a mensagem de exibição dividida em 5 linhas:
    // Linha 1: Raw, Linha 2: Short, Linha 3: # (número da nota)
    // Linha 4: Msg (tipo de mensagem) e Linha 5: Octave (nota com oitava)
    String displayMsg = "";
    displayMsg += "Raw: "      + String(rawStr.c_str())       + "\n";
    displayMsg += "Short: "    + String(shortStr.c_str())     + "\n";
    displayMsg += "#: "        + String(noteNumStr.c_str())   + "\n";
    displayMsg += "Msg: "      + String(messageStr.c_str())   + "\n";
    displayMsg += "Octave: "   + String(noteSoundOctaveStr.c_str());

    // Exibe a mensagem no display (incluindo frequência e amplitude)
    if (display != nullptr) {
      display->printMidiMessage(displayMsg.c_str(), frequency, amplitude);
    }

    lastMsgTime = millis();
    msgDisplayed = true;

    // Toca a nota se aplicável
    if (midiLength >= 3) {
      uint8_t note = midiData[1];
      uint8_t velocity = midiData[2];
      if (messageStr == "NoteOn" && velocity > 0) {
        audio.playNote(note, velocity);
      } else if (messageStr == "NoteOff" || (messageStr == "NoteOn" && velocity == 0)) {
        audio.stopNote();
      }
    }
  }

private:
  DisplayHandler* display;
};

DisplayHandler display;
MyESP32_Host_MIDI usbMidi(&display);

//
// Função callback para processar mensagens BLE MIDI
//
void onBLEMessage(const uint8_t *data, size_t length) {
  // Se o USB MIDI estiver ativo, priorizamos-o e ignoramos BLE
  if (usbMidi.isReady) {
    return;
  }

  const uint8_t* midiData = data + 1;  // Remove o cabeçalho BLE (assumindo formato semelhante)
  size_t midiLength = (length > 1) ? (length - 1) : 0;

  std::string rawStr             = MIDIHandler::getUsbMidiFormat(midiData, midiLength);
  std::string shortStr           = MIDIHandler::getShortFormat(midiData, midiLength);
  std::string noteNumStr         = MIDIHandler::getNoteNumberFormat(midiData, midiLength);
  std::string messageStr         = MIDIHandler::getMessageFormat(midiData, midiLength);
  std::string statusStr          = MIDIHandler::getMessageStatusFormat(midiData, midiLength);
  std::string noteSoundOctaveStr = MIDIHandler::getNoteSoundOctave(midiData, midiLength);

  float frequency = (midiLength >= 2) ? midiNoteToFrequency(midiData[1]) : 0.0;
  int amplitude = (midiLength >= 3) ? (int)(midiData[2] / 127.0 * 32767) : 0;

  String displayMsg = "";
  displayMsg += "Raw: "      + String(rawStr.c_str())       + "\n";
  displayMsg += "Short: "    + String(shortStr.c_str())     + "\n";
  displayMsg += "#: "        + String(noteNumStr.c_str())   + "\n";
  displayMsg += "Msg: "      + String(messageStr.c_str())   + "\n";
  displayMsg += "Octave: "   + String(noteSoundOctaveStr.c_str());

  display.printMidiMessage(displayMsg.c_str(), frequency, amplitude);

  lastMsgTime = millis();
  msgDisplayed = true;

  if (midiLength >= 3) {
    uint8_t note = midiData[1];
    uint8_t velocity = midiData[2];
    if (messageStr == "NoteOn" && velocity > 0) {
      audio.playNote(note, velocity);
    } else if (messageStr == "NoteOff" || (messageStr == "NoteOn" && velocity == 0)) {
      audio.stopNote();
    }
  }
}

//
// Setup: Inicializa USB MIDI, BLE MIDI, display e áudio
//
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32_Host_MIDI com MIDI USB, BLE, display e áudio");
  
  // Inicializa o display (configurado via DisplayHandler)
  display.init();

  // Inicializa a comunicação USB MIDI
  usbMidi.begin();
  
  // Inicializa o driver de áudio (PCM5102A)
  audio.begin();
  delay(100);  // Aguarda estabilização do DAC
  
  // Inicializa o BLE MIDI e configura o callback para mensagens BLE
  bleMIDI.begin();
  bleMIDI.setMidiMessageCallback(onBLEMessage);
}

void loop() {
  usbMidi.task();
  bleMIDI.task();
  audio.update();

  // Limpa o display se o timeout de exibição foi atingido
  if (msgDisplayed && (millis() - lastMsgTime > DISPLAY_TIMEOUT)) {
    display.clear();
    msgDisplayed = false;
  }

  delayMicroseconds(100);
}
