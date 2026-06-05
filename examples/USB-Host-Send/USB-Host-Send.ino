// ESP32_Host_MIDI / USB-Host-Send
// USB host MIDI 1.0: detect a connected keyboard, then send notes and CC to it.
//
// Requires: none beyond the board.
// Arduino IDE: Board ESP32-S3 (USB host) | USB Mode: Hardware CDC and JTAG
//
// Status LEDs: OUT_PIN5 = piano detected, OUT_PIN6 = activity blink,
// OUT_PIN7 = host started. A pedal on PEDAL_PIN sends sustain (CC64).

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;

#define PEDAL_PIN 4
#define OUT_PIN5 5 // piano detected
#define OUT_PIN6 6 // blink
#define OUT_PIN7 7 // host started

#define BLINK_PERIOD_MS 500
#define LOOP_DELAY_MS 10

#define NOTEON_DELAY_MS 200
#define NOTEOFF_DELAY_MS 300
#define RESET_TIMEOUT_MS 10000

bool hostStarted = false;
bool pianoDetected = false;

int pedalPressed = HIGH;
int pedalPrev = HIGH;

unsigned long hostStartMs = 0;
unsigned long detectMs = 0;
bool noteOnPending = false;
bool noteOffPending = false;
unsigned long noteOnMs = 0;

int blinkTimer = 0;
bool blinkState = false;

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

  midiHandler.addTransport(&usbHost);
  usbHost.begin();
  midiHandler.begin();
  hostStarted = true;
  hostStartMs = millis();
  digitalWrite(OUT_PIN7, HIGH);

  pedalPrev = digitalRead(PEDAL_PIN);
}

void loop() {
  if (blinkTimer >= BLINK_PERIOD_MS) {
    blinkTimer = 0;
    digitalWrite(OUT_PIN6, blinkState ? LOW : HIGH);
    blinkState = !blinkState;
  }

  if (hostStarted) {
    midiHandler.task();

    const auto &queue = midiHandler.getQueue();

    if (!queue.empty()) {
      if (!pianoDetected) {
        pianoDetected = true;
        detectMs = millis();
        noteOnPending = true;
        digitalWrite(OUT_PIN5, HIGH);
      }

      midiHandler.clearQueue();
    }

    // If the piano is not detected within 10 seconds, restart.
    if (!pianoDetected && (millis() - hostStartMs >= RESET_TIMEOUT_MS)) {
      ESP.restart();
    }

    if (pianoDetected && noteOnPending && (millis() - detectMs >= NOTEON_DELAY_MS)) {
      midiHandler.sendNoteOn(1, 60, 100);
      noteOnMs = millis();
      noteOnPending = false;
      noteOffPending = true;
    }

    if (pianoDetected && noteOffPending && (millis() - noteOnMs >= NOTEOFF_DELAY_MS)) {
      midiHandler.sendNoteOff(1, 60, 0);
      noteOffPending = false;
    }

    pedalPressed = digitalRead(PEDAL_PIN);

    if (pianoDetected && pedalPressed != pedalPrev) {
      if (pedalPressed == LOW) {
        midiHandler.sendControlChange(1, 64, 127);
      } else {
        midiHandler.sendControlChange(1, 64, 0);
      }

      pedalPrev = pedalPressed;
    }
  }

  blinkTimer += LOOP_DELAY_MS;
  delay(LOOP_DELAY_MS);
}
