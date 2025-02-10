#include "MIDI_handler.h"
#include <cstdio>    // Para sprintf
#include <cstdlib>

// Formato raw: [0x90, 0x3C, 0x64]
std::string MIDIHandler::getRawFormat(const uint8_t* data, size_t length) {
  std::string result = "[";
  char buf[10];
  for (size_t i = 0; i < length; i++) {
    sprintf(buf, "0x%02X", data[i]);
    result += buf;
    if (i < length - 1) {
      result += ", ";
    }
  }
  result += "]";
  return result;
}

// Formato short: "90 3C 64"
std::string MIDIHandler::getShortFormat(const uint8_t* data, size_t length) {
  std::string result;
  char buf[4];
  for (size_t i = 0; i < length; i++) {
    sprintf(buf, "%02X", data[i]);
    result += buf;
    if (i < length - 1) {
      result += " ";
    }
  }
  return result;
}

// Formato noteNumber: "60" (para Note On/Off) ou número do programa (para Program Change)
std::string MIDIHandler::getNoteNumberFormat(const uint8_t* data, size_t length) {
  if (length < 2) return "";
  char buf[10];
  sprintf(buf, "%d", data[1]);
  return std::string(buf);
}

// NOVO: Para Program Change, retorna o número do programa
std::string MIDIHandler::getProgramFormat(const uint8_t* data, size_t length) {
  // Program Change tem apenas 2 bytes: status e número do programa
  if (length < 2) return "";
  char buf[10];
  sprintf(buf, "%d", data[1]);
  return std::string(buf);
}

// Interpreta o status para identificar a mensagem
std::string MIDIHandler::getMessageFormat(const uint8_t* data, size_t length) {
  if (length < 1) return "";
  uint8_t status = data[0];
  uint8_t command = status & 0xF0;
  std::string message;
  switch (command) {
    case 0x80:
      message = "NoteOff";
      break;
    case 0x90:
      message = ((length >= 3 && data[2] == 0) ? "NoteOff" : "NoteOn");
      break;
    case 0xA0:
      message = "Polyphonic Aftertouch";
      break;
    case 0xB0:
      message = "Control Change";
      break;
    case 0xC0:
      message = "Prog. Change";
      break;
    case 0xD0:
      message = "Channel Aftertouch";
      break;
    case 0xE0:
      message = "Pitch Bend";
      break;
    default:
      message = "Unknown";
      break;
  }
  return message;
}

// Formato messageStatus: "9n" para Note On, "8n" para Note Off, etc.
std::string MIDIHandler::getMessageStatusFormat(const uint8_t* data, size_t length) {
  if (length < 1) return "";
  uint8_t status = data[0];
  uint8_t command = status & 0xF0;
  char buf[10];
  switch (command) {
    case 0x80:
      sprintf(buf, "8n");
      break;
    case 0x90:
      sprintf(buf, "9n");
      break;
    case 0xA0:
      sprintf(buf, "An");
      break;
    case 0xB0:
      sprintf(buf, "Bn");
      break;
    case 0xC0:
      sprintf(buf, "Cn");
      break;
    case 0xD0:
      sprintf(buf, "Dn");
      break;
    case 0xE0:
      sprintf(buf, "En");
      break;
    default:
      sprintf(buf, "Un");
      break;
  }
  return std::string(buf);
}

// Formato noteSound: retorna o nome da nota, ex: "C", "C#", "D", etc.
std::string MIDIHandler::getNoteSound(const uint8_t* data, size_t length) {
  if (length < 2) return "";
  int noteNumber = data[1];
  const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  int index = noteNumber % 12;
  return std::string(noteNames[index]);
}

// Formato noteSoundOctave: ex: "C5"
std::string MIDIHandler::getNoteSoundOctave(const uint8_t* data, size_t length) {
  if (length < 2) return "";
  int noteNumber = data[1];
  const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  int index = noteNumber % 12;
  int octave = (noteNumber / 12) - 1;
  char buf[10];
  sprintf(buf, "%s%d", noteNames[index], octave);
  return std::string(buf);
}

// Retorna um vetor formatado com os campos:
// { idMessage, channel, message, noteNumber, noteSound, noteSoundOctave, velocity }
// Para Program Change, noteNumber conterá o número do programa e os demais campos poderão ser vazios ou zero.
TypeVector MIDIHandler::getMessageVector(const uint8_t* data, size_t length) {
  TypeVector vec;
  if (length < 1) return vec;
  uint8_t status = data[0];
  int idMessage = status >> 4;         // Nibble alto
  int channel = status & 0x0F;           // Nibble baixo
  std::string message = getMessageFormat(data, length);
  int noteNumber = (length >= 2) ? data[1] : 0;
  std::string noteSound = (length >= 2) ? getNoteSound(data, length) : "";
  std::string noteSoundOctave = (length >= 2) ? getNoteSoundOctave(data, length) : "";
  int velocity = (length >= 3) ? data[2] : 0;

  // Se for Program Change, utilize getProgramFormat para obter o número do programa
  if ((status & 0xF0) == 0xC0) {
    noteNumber = (length >= 2) ? data[1] : 0;
    // Para mensagens de Program Change, os campos de noteSound, noteSoundOctave e velocity não se aplicam.
    noteSound = "";
    noteSoundOctave = "";
    velocity = 0;
  }

  vec.push_back(idMessage);
  vec.push_back(channel);
  vec.push_back(message);
  vec.push_back(noteNumber);
  vec.push_back(noteSound);
  vec.push_back(noteSoundOctave);
  vec.push_back(velocity);
  
  return vec;
}

std::string MIDIHandler::getUsbMidiFormat(const uint8_t* data, size_t length) {
  if (length < 1) return "[]";

  std::string result = "[";
  char buf[10];

  uint8_t status = data[0];
  uint8_t cin = 0x00; // Code Index Number

  // Definição do Code Index Number (CIN) conforme o status MIDI
  if ((status & 0xF0) == 0x80) cin = 0x08;  // Note Off
  else if ((status & 0xF0) == 0x90) cin = 0x09;  // Note On
  else if ((status & 0xF0) == 0xA0) cin = 0x0A;  // Polyphonic Key Pressure
  else if ((status & 0xF0) == 0xB0) cin = 0x0B;  // Control Change
  else if ((status & 0xF0) == 0xC0) cin = 0x0C;  // Program Change
  else if ((status & 0xF0) == 0xD0) cin = 0x0D;  // Channel Pressure
  else if ((status & 0xF0) == 0xE0) cin = 0x0E;  // Pitch Bend
  else if (status == 0xF0) cin = 0x04;  // System Exclusive (SysEx Start)
  else if (status == 0xF7) cin = 0x05;  // SysEx End
  else if (status == 0xF2) cin = 0x02;  // Song Position Pointer
  else if (status == 0xF3) cin = 0x03;  // Song Select
  else if (status == 0xF6) cin = 0x05;  // Tune Request
  else if (status >= 0xF8) cin = 0x0F;  // System Real-Time Messages

  uint8_t usbHeader = (0x00 << 4) | cin; // Cable Number 0x00 + CIN calculado

  sprintf(buf, "0x%02X", usbHeader);
  result += buf;
  result += ", ";

  for (size_t i = 0; i < length; i++) {
    sprintf(buf, "0x%02X", data[i]);
    result += buf;
    if (i < length - 1) {
      result += ", ";
    }
  }

  result += "]";
  return result;
}

