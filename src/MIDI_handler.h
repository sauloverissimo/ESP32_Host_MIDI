#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <Arduino.h>
#include "datahandler.h"  // Inclui os tipos: TypeElement, TypeVector, etc.
#include <string>
#include <vector>

/*
  MIDIHandler:
  Conjunto de métodos estáticos para interpretar mensagens MIDI "brutas"
  e retornar os dados em vários formatos.
  
  Formatos disponíveis:
  - Raw:          [0x90, 0x3C, 0x64]
  - Short:        "90 3C 64"
  - NoteNumber:   "60"
  - Message:      "NoteOn", "NoteOff", "Control Change", "Program Change", etc.
  - MessageStatus:"9n"
  - NoteSound:    "C"
  - NoteSoundOctave: "C5"
  - ProgramFormat: Número do programa (para mensagens de Program Change)
  
  E também o formato vetor:
  { idMessage, channel, message, noteNumber, noteSound, noteSoundOctave, velocity }
  (Para Program Change, noteNumber conterá o número do programa e os demais campos podem ser vazios ou zero.)
*/

class MIDIHandler {
public:
  // Retorna o formato "raw": [0x90, 0x3C, 0x64]
  static std::string getRawFormat(const uint8_t* data, size_t length);

  // Retorna o formato "short": "90 3C 64"
  static std::string getShortFormat(const uint8_t* data, size_t length);

  // Retorna o número da nota (ou programa) em formato decimal, por exemplo: "60"
  static std::string getNoteNumberFormat(const uint8_t* data, size_t length);

  // Retorna o nome da mensagem, por exemplo: "NoteOn", "NoteOff", "Control Change", "Program Change"
  static std::string getMessageFormat(const uint8_t* data, size_t length);

  // Retorna o status da mensagem, por exemplo: "9n" para Note On, "8n" para Note Off, etc.
  static std::string getMessageStatusFormat(const uint8_t* data, size_t length);

  // Retorna o som da nota, por exemplo: "C", "C#", "D", etc.
  static std::string getNoteSound(const uint8_t* data, size_t length);

  // Retorna o som da nota com oitava, por exemplo "C5"
  static std::string getNoteSoundOctave(const uint8_t* data, size_t length);

  // NOVO: Retorna o número do programa (para mensagens de Program Change)
  static std::string getProgramFormat(const uint8_t* data, size_t length);

  // Retorna um vetor formatado (TypeVector) com os campos:
  // { idMessage, channel, message, noteNumber, noteSound, noteSoundOctave, velocity }
  // Para Program Change, noteNumber conterá o número do programa e os demais campos poderão ser vazios ou zero.
  static TypeVector getMessageVector(const uint8_t* data, size_t length);

  // NOVO: Retorna a mensagem no formato USB MIDI (com cabeçalho de 4 bytes)
  static std::string getUsbMidiFormat(const uint8_t* data, size_t length);
};

#endif // MIDI_HANDLER_H
