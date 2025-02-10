#ifndef ESP32_PCM5102A_MIDI_H
#define ESP32_PCM5102A_MIDI_H

#include <Arduino.h>

class ESP32_PCM5102A_MIDI {
public:
  ESP32_PCM5102A_MIDI();
  ~ESP32_PCM5102A_MIDI();

  void begin();
  void playNote(uint8_t note, uint8_t velocity);
  void stopNote();
  void update();
  
  bool isNoteActive() const { return noteActive; }  // Novo método público

private:
  bool noteActive;
  float frequency;
  float phase;
  float phaseIncrement;
  int amplitude;

  static const int sampleRate = 44100;
  static const int bufferSize = 64;

  float midiNoteToFrequency(uint8_t note);
};

#endif // ESP32_PCM5102A_MIDI_H
