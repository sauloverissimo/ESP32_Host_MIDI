# 🚀 Primeiros Passos

Neste guia você terá o ESP32 recebendo e enviando MIDI em menos de 5 minutos.

---

## Pré-requisito

- Biblioteca instalada (veja [Instalação](instalacao.md))
- ESP32-S3 com cabo USB-OTG **ou** qualquer ESP32 com Bluetooth

---

## Passo 1 — O Sketch Mais Simples

Este sketch imprime todos os eventos MIDI recebidos via USB Host ou BLE no Serial Monitor:

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>   // v6.0+: cada transporte é explícito
#include <BLEConnection.h>
// Arduino IDE: Tools > USB Mode → "USB Host"

USBConnection usbHost;   // (1) instâncias globais (TinyUSB precisa antes de USB.begin)
BLEConnection bleHost;

void setup() {
    Serial.begin(115200);
    midiHandler.addTransport(&usbHost);   // (2) registra cada transporte
    midiHandler.addTransport(&bleHost);
    usbHost.begin();                       // (3) o user controla o lifecycle
    bleHost.begin("ESP32 MIDI BLE");
    midiHandler.begin();                   // (4)
}

void loop() {
    midiHandler.task();                    // (5)

    for (const auto& ev : midiHandler.getQueue()) {  // (6)
        char noteBuf[8];
        Serial.printf("%-12s %-5s ch=%d  vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),    // "NoteOn" | "NoteOff" | "ControlChange"...
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), // "C4", "D#5"...
            ev.channel0 + 1,
            ev.velocity7);
    }
}
```

**Anotações:**

1. Em v6+ os transportes (`USBConnection`, `BLEConnection`, `UARTConnection`, etc.) são instanciados explicitamente. Inclua só os headers que vai usar.
2. `addTransport(&t)` registra cada um no `MIDIHandler` antes do `begin()`.
3. Cada transporte controla seu próprio `begin()`: `usbHost.begin()` (sem nome), `bleHost.begin("nome")`, `uart.begin(...)`.
4. `midiHandler.begin()` aplica config e prepara o queue. Não inicia transportes nenhum.
5. `task()` deve ser chamado em todo `loop()`, drena os ring buffers de todos os transportes registrados.
6. `getQueue()` retorna a fila de eventos desde a última chamada de `task()`.

---

## Passo 2 — Acessar Campos do Evento

Cada evento tem os seguintes campos:

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    // Identificação
    ev.index;        // Contador global de eventos (único, crescente)
    ev.timestamp;    // millis() no momento da chegada
    ev.delay;        // Δt em ms desde o evento anterior

    // Tipo de mensagem
    ev.statusCode;   // MIDIStatus enum: MIDI_NOTE_ON, MIDI_NOTE_OFF, MIDI_CONTROL_CHANGE...
    ev.channel0;     // Canal MIDI: 0–15 (some +1 para exibir 1–16)

    // Nota (apenas NoteOn / NoteOff)
    ev.noteNumber;   // Número MIDI: 0–127 (60 = C4 = Dó central)
    ev.velocity7;    // Velocidade 7-bit: 0–127 (também: valor CC, program, pressure)
    ev.velocity16;   // Velocidade 16-bit: 0–65535 (MIDI 2.0)

    // Helpers estáticos (zero allocation)
    // MIDIHandler::statusName(ev.statusCode)              → "NoteOn", "NoteOff"...
    // MIDIHandler::noteName(ev.noteNumber)                → "C", "C#", "D"...
    // MIDIHandler::noteWithOctave(ev.noteNumber, buf, sz) → "C4", "D#5"...

    // Agrupamento de acordes
    ev.chordIndex;   // Notas simultâneas compartilham o mesmo chordIndex

    // Pitch Bend (apenas PitchBend)
    ev.pitchBend14;  // 0–16383 (centro = 8192), MIDI 1.0
    ev.pitchBend32;  // 0–4294967295 (centro = 2147483648), MIDI 2.0
}
```

### Exemplo — Apenas NoteOn

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    if (ev.statusCode == MIDI_NOTE_ON && ev.velocity7 > 0) {
        char noteBuf[8];
        Serial.printf("Nota: %s  Velocidade: %d  Canal: %d\n",
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.velocity7,
            ev.channel0 + 1);
    }
}
```

### Exemplo — Control Change

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    if (ev.statusCode == MIDI_CONTROL_CHANGE) {
        // ev.noteNumber = número do controlador (CC#)
        // ev.velocity7 = valor do controlador (0–127)
        Serial.printf("CC #%d = %d  (canal %d)\n",
            ev.noteNumber, ev.velocity7, ev.channel0 + 1);
    }
}
```

---

## Passo 3 — Enviar MIDI de Volta

Os métodos de envio fazem **fan-out**: tentam cada transporte registrado em ordem e retornam `true` no primeiro que aceitar. Não é broadcast simultâneo.

```cpp
// NoteOn: canal 1, nota C4 (60), velocidade 100
midiHandler.sendNoteOn(1, 60, 100);

// NoteOff: canal 1, nota C4 (60), velocidade 0
midiHandler.sendNoteOff(1, 60, 0);

// Control Change: canal 1, CC #7 (volume) = 127
midiHandler.sendControlChange(1, 7, 127);

// Program Change: canal 1, programa 0
midiHandler.sendProgramChange(1, 0);

// Pitch Bend: canal 1, valor -8192 a +8191 (0 = centro)
midiHandler.sendPitchBend(1, 4096);  // +0.5 semitom
```

### Exemplo — Echo MIDI (loopback)

Recebe qualquer NoteOn e reenvia para todos os transportes:

```cpp
void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        if (ev.statusCode == MIDI_NOTE_ON) {
            // Reenvia com velocidade dobrada (limitada a 127)
            uint8_t vel = min((int)(ev.velocity7 * 2), 127);
            midiHandler.sendNoteOn(ev.channel0 + 1, ev.noteNumber, vel);
        }
        if (ev.statusCode == MIDI_NOTE_OFF) {
            midiHandler.sendNoteOff(ev.channel0 + 1, ev.noteNumber, 0);
        }
    }
}
```

---

## Passo 4 — Verificar Conexão BLE

```cpp
void loop() {
    midiHandler.task();

    // v6.0+: consultar a instância de BLEConnection diretamente.
    // MIDIHandler::isBleConnected() foi removido junto com o member built-in.
    if (bleHost.isConnected()) {
        Serial.println("BLE MIDI conectado!");
    }
}
```

!!! tip "Conectar via iOS"
    1. Abra **GarageBand** no iPhone
    2. Toque em "+" → Música Mixada → Iniciar
    3. Menu de configurações → Conectar dispositivo MIDI via Bluetooth
    4. O ESP32 aparecerá como "ESP32 MIDI BLE" (ou o nome configurado)

---

## Passo 5 — Sketch Completo com USB + BLE

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>
#include <BLEConnection.h>
// Arduino IDE: Tools > USB Mode → "USB Host"

USBConnection usbHost;
BLEConnection bleHost;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 Host MIDI — Iniciando...");

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 20;         // capacidade da fila
    cfg.chordTimeWindow = 50;   // ms para agrupar acordes
    midiHandler.addTransport(&usbHost);
    midiHandler.addTransport(&bleHost);
    usbHost.begin();
    bleHost.begin("ESP32 MIDI BLE");
    midiHandler.begin(cfg);

    Serial.println("Pronto! Conecte um teclado USB ou use BLE.");
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        if (ev.statusCode == MIDI_NOTE_ON && ev.velocity7 > 0) {
            Serial.printf("[NoteOn]  %s  vel=%3d  ch=%d  t=%lums\n",
                MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
                ev.velocity7,
                ev.channel0 + 1,
                ev.timestamp);
        } else if (ev.statusCode == MIDI_NOTE_OFF || ev.velocity7 == 0) {
            Serial.printf("[NoteOff] %s             ch=%d\n",
                MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
                ev.channel0 + 1);
        } else if (ev.statusCode == MIDI_CONTROL_CHANGE) {
            Serial.printf("[CC]      #%3d = %3d    ch=%d\n",
                ev.noteNumber, ev.velocity7, ev.channel0 + 1);
        } else if (ev.statusCode == MIDI_PITCH_BEND) {
            Serial.printf("[Pitch]   %d (centro=8192)  ch=%d\n",
                ev.pitchBend14, ev.channel0 + 1);
        }
    }
}
```

---

## Fluxo de Inicialização (v6.0+)

```mermaid
sequenceDiagram
    participant SKETCH as Seu Sketch
    participant HANDLER as MIDIHandler
    participant USB as USBConnection
    participant BLE as BLEConnection

    SKETCH->>USB: USBConnection usbHost; (global)
    SKETCH->>BLE: BLEConnection bleHost; (global)
    SKETCH->>HANDLER: addTransport(&usbHost)
    SKETCH->>HANDLER: addTransport(&bleHost)
    SKETCH->>USB: usbHost.begin()
    Note over USB: Inicia FreeRTOS task\nno Core 0
    SKETCH->>BLE: bleHost.begin("nome")
    Note over BLE: Inicia stack BLE\ne advertising
    SKETCH->>HANDLER: midiHandler.begin(cfg)
    HANDLER-->>SKETCH: Pronto

    loop Cada loop()
        SKETCH->>HANDLER: midiHandler.task()
        HANDLER->>USB: usbHost.task()
        HANDLER->>BLE: bleHost.task()
        USB-->>HANDLER: Eventos do ring buffer
        BLE-->>HANDLER: Eventos do ring buffer
        HANDLER-->>SKETCH: getQueue() com eventos
    end
```

---

## Próximos Passos

- [Configuração →](configuracao.md) — ajustar `MIDIHandlerConfig`, filtros e histórico
- [Transportes →](../transportes/visao-geral.md) — adicionar mais transportes (RTP-MIDI, UART, OSC...)
- [Funcionalidades →](../funcionalidades/deteccao-acordes.md) — detecção de acordes, notas ativas
