#include <Arduino.h>
#include <ESP32_Host_MIDI.h>

#define PEDAL_PIN 4
#define OUT_PIN5 5 // piano detectado
#define OUT_PIN6 6 // titila
#define OUT_PIN7 7 // host iniciado

#define FREC_TITILAR 500
#define FREC_LOOP 10

#define NOTEON_DELAY_MS 200
#define NOTEOFF_DELAY_MS 300
#define RESET_TIMEOUT_MS 10000

bool hostIniciado = false;
bool pianoDetectado = false;

int pedalPresionado = HIGH;
int pedalAnterior = HIGH;

unsigned long momentoInicioHost = 0;
unsigned long momentoDeteccion = 0;
bool noteOnPendiente = false;
bool noteOffPendiente = false;
unsigned long momentoNoteOn = 0;

int timerTitilar = 0;
bool estadoTitilar = false;

void setup() {
  pinMode(PEDAL_PIN, INPUT_PULLUP);
  pinMode(OUT_PIN5, OUTPUT);
  pinMode(OUT_PIN6, OUTPUT);
  pinMode(OUT_PIN7, OUTPUT);

  digitalWrite(OUT_PIN5, HIGH);
  digitalWrite(OUT_PIN6, HIGH);
  digitalWrite(OUT_PIN7, HIGH);
  delay(1000);
  digitalWrite(OUT_PIN5, LOW);
  digitalWrite(OUT_PIN6, LOW);
  digitalWrite(OUT_PIN7, LOW);

  midiHandler.begin();
  hostIniciado = true;
  momentoInicioHost = millis();
  digitalWrite(OUT_PIN7, HIGH);

  pedalAnterior = digitalRead(PEDAL_PIN);
}

void loop() {
  if (timerTitilar >= FREC_TITILAR) {
    timerTitilar = 0;
    digitalWrite(OUT_PIN6, estadoTitilar ? LOW : HIGH);
    estadoTitilar = !estadoTitilar;
  }

  if (hostIniciado) {
    midiHandler.task();

    const auto &queue = midiHandler.getQueue();

    if (!queue.empty()) {
      if (!pianoDetectado) {
        pianoDetectado = true;
        momentoDeteccion = millis();
        noteOnPendiente = true;
        digitalWrite(OUT_PIN5, HIGH);
      }

      midiHandler.clearQueue();
    }

    // Si en 10 segundos no detectó al piano, reiniciar
    if (!pianoDetectado && (millis() - momentoInicioHost >= RESET_TIMEOUT_MS)) {
      ESP.restart();
    }

    if (pianoDetectado && noteOnPendiente && (millis() - momentoDeteccion >= NOTEON_DELAY_MS)) {
      midiHandler.sendNoteOn(1, 60, 100);
      momentoNoteOn = millis();
      noteOnPendiente = false;
      noteOffPendiente = true;
    }

    if (pianoDetectado && noteOffPendiente && (millis() - momentoNoteOn >= NOTEOFF_DELAY_MS)) {
      midiHandler.sendNoteOff(1, 60, 0);
      noteOffPendiente = false;
    }

    pedalPresionado = digitalRead(PEDAL_PIN);

    if (pianoDetectado && pedalPresionado != pedalAnterior) {
      if (pedalPresionado == LOW) {
        midiHandler.sendControlChange(1, 64, 127);
      } else {
        midiHandler.sendControlChange(1, 64, 0);
      }

      pedalAnterior = pedalPresionado;
    }
  }

  timerTitilar += FREC_LOOP;
  delay(FREC_LOOP);
}
