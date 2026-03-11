# 🎹 UART MIDI Básico

O exemplo `UART-MIDI-Basic` é o sketch mais simples da biblioteca — MIDI serial via DIN-5 impresso no Serial Monitor. Ideal para testar a instalação sem hardware USB-OTG.

---

## Hardware Necessário

| Componente | Detalhe |
|-----------|---------|
| Placa | Qualquer ESP32 |
| MIDI IN | Optoacoplador (6N138, PC-900V) + DIN-5 |
| MIDI OUT | 2× resistores 220Ω + DIN-5 (opcional) |

---

## Circuito MIDI IN

```
DIN-5 pino 5 ─── 220Ω ───► 6N138 (pino 2, LED +)
DIN-5 pino 4 ─────────────► 6N138 (pino 3, LED -)
DIN-5 pino 2 ─────────────► GND

6N138 pino 8 (VCC) ───► 3.3V
6N138 pino 6 (Vout) ──► ESP32 GPIO RX (pull-up interno)
6N138 pino 4 (GND) ───► GND
```

!!! warning "Isolação obrigatória"
    Conectar DIN-5 diretamente ao GPIO sem optoacoplador pode danificar o ESP32. Sempre use isolação óptica para MIDI IN.

---

## Código Completo

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"

// Pinos MIDI
#define MIDI_RX_PIN 16   // Conectado ao optoacoplador
#define MIDI_TX_PIN 17   // Conectado ao DIN-5 MIDI OUT (via 220Ω)

UARTConnection uartMIDI;

void setup() {
    Serial.begin(115200);
    Serial.println("UART MIDI Basic — ESP32_Host_MIDI");
    Serial.println("-----------------------------------");

    // Iniciar UART MIDI (31250 baud)
    uartMIDI.begin(Serial1, MIDI_RX_PIN, MIDI_TX_PIN);

    // Registrar e iniciar o handler
    midiHandler.addTransport(&uartMIDI);
    midiHandler.begin();

    Serial.println("Pronto! Aguardando MIDI via DIN-5...");
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        // Imprimir evento formatado
        if (ev.statusCode == MIDIHandler::NoteOn && ev.velocity7 > 0) {
            char noteBuf[8];
            Serial.printf("[NoteOn]  %-5s  canal=%d  vel=%3d  t=%lu ms\n",
                MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), ev.channel0 + 1, ev.velocity7, ev.timestamp);

            // Teste de NoteOff automático após 100ms (TX)
            delay(100);
            midiHandler.sendNoteOff(ev.channel0 + 1, ev.noteNumber, 0);

        } else if (ev.statusCode == MIDIHandler::NoteOff) {
            char noteBuf[8];
            Serial.printf("[NoteOff] %-5s  canal=%d\n",
                MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), ev.channel0 + 1);

        } else if (ev.statusCode == MIDIHandler::ControlChange) {
            Serial.printf("[CC]      #%-3d = %3d  canal=%d\n",
                ev.noteNumber, ev.velocity7, ev.channel0 + 1);

        } else if (ev.statusCode == MIDIHandler::PitchBend) {
            Serial.printf("[PitchBend] %d  canal=%d\n",
                ev.pitchBend14, ev.channel0 + 1);
        }
    }
}
```

---

## Saída do Serial Monitor

Ao tocar um acorde de Dó maior em um teclado MIDI:

```
UART MIDI Basic — ESP32_Host_MIDI
-----------------------------------
Pronto! Aguardando MIDI via DIN-5...
[NoteOn]  C4    canal=1  vel=100  t=1234 ms
[NoteOn]  E4    canal=1  vel= 95  t=1235 ms
[NoteOn]  G4    canal=1  vel=110  t=1236 ms
[NoteOff] C4    canal=1
[NoteOff] E4    canal=1
[NoteOff] G4    canal=1
[CC]      #7  = 127  canal=1
```

---

## Teste de TX (MIDI OUT)

Para testar o MIDI OUT, envie notas programaticamente:

```cpp
// Adicionar ao loop() para tocar uma escala de Dó
static unsigned long lastNote = 0;
static int noteIdx = 0;
const int ESCALA[] = {60, 62, 64, 65, 67, 69, 71, 72};  // C-D-E-F-G-A-B-C

if (millis() - lastNote > 500) {
    // Desliga nota anterior
    if (noteIdx > 0) {
        midiHandler.sendNoteOff(1, ESCALA[(noteIdx - 1) % 8], 0);
    }
    // Liga próxima nota
    midiHandler.sendNoteOn(1, ESCALA[noteIdx % 8], 100);
    noteIdx++;
    lastNote = millis();
}
```

---

## Pinos por Placa

| Placa | RX (GPIO) | TX (GPIO) |
|-------|----------|----------|
| ESP32 DevKit | 16 | 17 |
| ESP32-S3 DevKit | 18 | 17 |
| LilyGO T-Display-S3 | 18 | 17 |
| ESP32-C3 | 4 | 5 |
| ESP32-P4 (UART1) | 16 | 17 |

---

## Próximos Passos

- [UART / DIN-5 →](../transportes/uart-din5.md) — detalhes do transporte e circuito
- [RTP-MIDI WiFi →](rtp-midi-wifi.md) — combinar UART com Apple MIDI
- [T-Display-S3 →](t-display-s3.md) — adicionar display ao projeto
