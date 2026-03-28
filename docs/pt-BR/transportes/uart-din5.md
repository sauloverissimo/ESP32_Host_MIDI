# 🎹 UART / DIN-5

MIDI serial padrão (31250 baud, 8N1) para conectar **hardware vintage** — sintetizadores, caixas de ritmo, mixers e sequenciadores com conector DIN-5. Funciona em qualquer ESP32.

---

## Protocolo MIDI Serial

O MIDI original (1983) usa comunicação serial assíncrona:

| Parâmetro | Valor |
|-----------|-------|
| Baud rate | **31250 bps** (não é 31.25 kHz, é 31250 bps) |
| Formato | 8 bits, sem paridade, 1 stop bit (8N1) |
| Nível elétrico | 5V TTL com isolação óptica (opto) |
| Conector | DIN-5 fêmea (5 pinos, mas usa apenas 3: GND, TX, RX) |

---

## Pinagem do Conector DIN-5

```
        DIN-5 (vista frontal do conector fêmea)
             ___
           /     \
          / 4   5 \
         |  1   2  |
          \   3   /
           \_____/

DIN-5 pino 2 = GND (blindagem)
DIN-5 pino 4 = MIDI OUT (+5V fonte de corrente)
DIN-5 pino 5 = MIDI IN (dados)
```

---

## Circuito de Hardware

### MIDI OUT (ESP32 → Sintetizador)

```
ESP32 GPIO TX ─── 220Ω ──► DIN-5 pino 5 (dados)
3.3V / 5V     ─── 220Ω ──► DIN-5 pino 4 (fonte)
GND           ──────────► DIN-5 pino 2
```

!!! tip "Nível de tensão"
    O MIDI OUT não requer optoacoplador. Os dois resistores de 220Ω limitam a corrente para o LED interno do optoacoplador no dispositivo receptor.

### MIDI IN (Sintetizador → ESP32)

!!! warning "Isolação obrigatória"
    O MIDI IN **deve** usar um optoacoplador para isolar eletricamente o ESP32 do instrumento. Conectar diretamente pode danificar o ESP32 por diferença de potencial de terra.

```
DIN-5 pino 5 ─── 220Ω ─── Optoacoplador (LED)
DIN-5 pino 4 ─────────────── Optoacoplador (anodo)
DIN-5 pino 2 ──────────────────────────────── GND

Optoacoplador (fototransistor):
  Coletor ─── 3.3V
  Emissor ─── 1kΩ ─── ESP32 GPIO RX
```

**Optoacopladores recomendados:**

| Componente | Notas |
|-----------|-------|
| **PC-900V** | Recomendado pela MMA para MIDI |
| **6N138** | Mais comum, disponível no Brasil |
| **TLP2361** | Rápido, 5V, ótima escolha |
| **H11L1** | Alternativa econômica |

### Esquema Completo

```
                    MIDI IN
DIN-5 pino 5 ─┬─ 220Ω ─► LED (+) do 6N138
DIN-5 pino 4 ─┘           LED (-) do 6N138
DIN-5 pino 2 ──────────── GND

6N138 saída:
  VCC (pino 8) ─── 3.3V
  Vout (pino 6) ─── ESP32 RX (pull-up interno)
  GND (pino 4) ─── GND

                    MIDI OUT
ESP32 TX ─── 220Ω ─── DIN-5 pino 5
3.3V     ─── 220Ω ─── DIN-5 pino 4
GND      ─────────── DIN-5 pino 2
```

---

## Código

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"

UARTConnection uartMIDI;

void setup() {
    Serial.begin(115200);

    // Serial1: RX = GPIO 16, TX = GPIO 17
    uartMIDI.begin(Serial1, /*RX=*/16, /*TX=*/17);

    midiHandler.addTransport(&uartMIDI);
    midiHandler.begin();

    Serial.println("UART MIDI pronto (31250 baud)");
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[DIN-5] %s %s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.channel0 + 1,
            ev.velocity7);
    }
}
```

---

## Múltiplos UARTs (ESP32-P4)

O ESP32-P4 tem **5 UARTs hardware**, permitindo múltiplas portas DIN-5 simultâneas:

```cpp
#include "src/UARTConnection.h"

UARTConnection uart1;  // MIDI IN/OUT porta 1
UARTConnection uart2;  // MIDI IN/OUT porta 2

void setup() {
    uart1.begin(Serial1, /*RX=*/16, /*TX=*/17);
    uart2.begin(Serial2, /*RX=*/18, /*TX=*/19);

    midiHandler.addTransport(&uart1);
    midiHandler.addTransport(&uart2);
    midiHandler.begin();
}
```

---

## Mensagens MIDI Suportadas

| Mensagem | Suporte |
|---------|---------|
| NoteOn / NoteOff | ✅ |
| Control Change (CC) | ✅ |
| Program Change | ✅ |
| Pitch Bend | ✅ |
| Channel Pressure | ✅ |
| MIDI Clock / Start / Stop | ✅ (real-time messages) |
| Running Status | ✅ (processado automaticamente) |
| SysEx | ❌ (ignorado) |

---

## Pinos Disponíveis por Chip

| Chip | UARTs hardware | Pinos sugeridos (RX/TX) |
|------|---------------|------------------------|
| ESP32 clássico | 3 | UART1: 16/17, UART2: 4/2 |
| ESP32-S3 | 3 | UART1: 18/17, UART2: 19/20 |
| ESP32-S2 | 2 | UART1: 18/17 |
| ESP32-P4 | **5** | UART1: 16/17, UART2: 18/19, ... |
| ESP32-C3 | 2 | UART1: 4/5 |

!!! tip "Evitar GPIO 0"
    Não use GPIO 0 para UART MIDI — ele é o botão de boot e pode causar comportamento inesperado.

---

## Exemplos

| Exemplo | Descrição |
|---------|-----------|
| `UART-MIDI-Basic` | DIN-5 entrada → Serial Monitor |
| `P4-Dual-UART-MIDI` | Dois UARTs no ESP32-P4 |

---

## Bridge DIN-5 ↔ WiFi

Um caso de uso clássico: conectar um sintetizador vintage ao macOS via WiFi:

```
Sintetizador DIN-5 → ESP32 UART → MIDIHandler → RTP-MIDI → macOS Logic Pro
```

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"
#include "src/RTPMIDIConnection.h"

UARTConnection uartMIDI;
RTPMIDIConnection rtpMIDI;

void setup() {
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(500);

    uartMIDI.begin(Serial1, 16, 17);
    rtpMIDI.begin("Synth Bridge");

    midiHandler.addTransport(&uartMIDI);
    midiHandler.addTransport(&rtpMIDI);
    midiHandler.begin();
    // Pronto! DIN-5 ↔ WiFi bridge automático
}
```

---

## Próximos Passos

- [RTP-MIDI →](rtp-midi.md) — combinar UART com WiFi Apple MIDI
- [ESP-NOW →](esp-now.md) — bridge DIN-5 ↔ ESP-NOW mesh
- [Exemplos UART →](../exemplos/uart-basico.md) — sketch completo comentado
