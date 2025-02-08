#include <Arduino.h>
#include "ESP32_Host_MIDI.h"         // Biblioteca USB MIDI
#include "displayhandler.h"          // Handler para exibição no display (LovyanGFX)
#include "MIDI_handler.h"            // Módulo para interpretar as mensagens MIDI
#include "MIDI_PCM5102A_Handler.h"   // Biblioteca para tocar notas via DAC PCM5102A
#include <PCA95x5.h>                // Biblioteca para expander de I/O

#define DISPLAY_TIMEOUT 1000  // Tempo de exibição do display (1 segundo)

unsigned long lastMsgTime = 0;
bool msgDisplayed = false;

// Instância para reprodução de áudio via PCM5102A
MIDI_PCM5102A_Handler audio;

// Definição única do expander PCA95x5, usando o template correto:
//PCA95x5::PCA95x5<> ioex;

// Classe derivada que processa as mensagens MIDI, exibe no display e reproduz áudio
class MyESP32_Host_MIDI : public ESP32_Host_MIDI {
public:
  MyESP32_Host_MIDI(DisplayHandler* disp) : display(disp) {}

  void onMidiMessage(const uint8_t *data, size_t length) override {
    const uint8_t* midiData = data + 1;
    size_t midiLength = (length > 1) ? (length - 1) : 0;

    std::string rawStr             = MIDIHandler::getRawFormat(midiData, midiLength);
    std::string shortStr           = MIDIHandler::getShortFormat(midiData, midiLength);
    std::string noteNumStr         = MIDIHandler::getNoteNumberFormat(midiData, midiLength);
    std::string messageStr         = MIDIHandler::getMessageFormat(midiData, midiLength);
    std::string noteSoundOctaveStr = MIDIHandler::getNoteSoundOctave(midiData, midiLength);

    String displayMsg = "";
    displayMsg += "Raw: "    + String(rawStr.c_str())       + "\n";
    displayMsg += "Short: "  + String(shortStr.c_str())     + "\n";
    displayMsg += "Note#: "  + String(noteNumStr.c_str())   + "\n";
    displayMsg += "Msg: "    + String(messageStr.c_str())   + "\n";
    displayMsg += "Octave: " + String(noteSoundOctaveStr.c_str());

    if (display) {
      display->printMidiMessage(displayMsg.c_str());
    }

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

private:
  DisplayHandler* display;
};

DisplayHandler display;
MyESP32_Host_MIDI usbMidi(&display);

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32_Host_MIDI com áudio, display e controle do DAC PCM5102A");

  display.init();
  usbMidi.begin();

  // Inicializa o expander PCA95x5 para controle do mute
  ioex.attach(Wire, 0x20);
  ioex.direction(PCA95x5::Direction::OUT_ALL);
  ioex.write(PCM5102A_MUTE_PIN, PCA95x5::Level::H);
  Serial.println("PCM5102A Desmutado!");

  audio.begin();
}

void loop() {
  usbMidi.task();
  audio.update();

  if (msgDisplayed && (millis() - lastMsgTime > DISPLAY_TIMEOUT)) {
    display.clear();
    msgDisplayed = false;
  }

  delay(10);
}
