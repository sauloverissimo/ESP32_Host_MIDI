# 🎛️ Introdução

**ESP32_Host_MIDI** é uma biblioteca Arduino de código aberto que transforma o ESP32 em um hub MIDI universal com suporte a **9 transportes simultâneos**, todos operando pela mesma API limpa de eventos.

---

## O Que É a Biblioteca

A ideia central é simples: não importa de onde o MIDI vem — USB, Bluetooth, WiFi, cabo serial, rádio — ele chega sempre na mesma fila de eventos (`getQueue()`), com o mesmo formato (`MIDIEventData`), pronto para processar.

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    ev.status;      // "NoteOn", "NoteOff", "ControlChange", "PitchBend"...
    ev.channel;     // 1–16
    ev.note;        // número MIDI (0–127)
    ev.noteOctave;  // "C4", "D#5", "G3"...
    ev.velocity;    // 0–127
    ev.timestamp;   // millis() na chegada
    ev.chordIndex;  // agrupa notas simultâneas
}
```

Ao mesmo tempo, `midiHandler.sendNoteOn()` e outros métodos de envio transmitem para **todos** os transportes ativos simultaneamente. Um evento que chega pelo USB pode sair imediatamente pelo BLE, pelo DIN-5 e pelo WiFi — sem nenhum código extra.

---

## Os 9 Transportes

```mermaid
mindmap
  root((ESP32\nHost MIDI))
    USB Host
      ESP32-S3 / S2 / P4
      Teclados class-compliant
      Latência < 1 ms
    BLE MIDI
      Qualquer ESP32 com BT
      iOS · macOS · Android
      Latência 3-15 ms
    USB Device
      ESP32-S3 / S2 / P4
      Aparece como interface USB
      Latência < 1 ms
    ESP-NOW
      Qualquer ESP32
      Mesh P2P sem router
      Latência 1-5 ms
    RTP-MIDI WiFi
      AppleMIDI / RFC 6295
      macOS · iOS · Logic Pro
      Latência 5-20 ms
    Ethernet MIDI
      W5500 SPI ou P4 nativo
      Ideal para estúdios
      Latência 2-10 ms
    OSC
      WiFi UDP bidirecional
      Max/MSP · PD · SC
      Latência 5-15 ms
    UART / DIN-5
      Qualquer ESP32
      Sintetizadores vintage
      Latência < 1 ms
```

---

## Arquitetura de Software

### Separação por Cores do FreeRTOS

O ESP32 tem dois núcleos. A biblioteca usa essa separação para garantir baixa latência:

```mermaid
graph TD
    subgraph CORE0["🔵 Core 0 — Drivers e Stack"]
        USB_TASK["USB Host Task\n(USBConnection)"]
        BLE_STACK["Pilha BLE\n(BLEConnection)"]
        WIFI["Pilha WiFi / Ethernet\n(RTP-MIDI, OSC, ESP-NOW)"]
    end

    subgraph CORE1["🟢 Core 1 — Seu Código"]
        LOOP["loop()"]
        TASK["midiHandler.task()"]
        USER["Seu código\n(display, synth, etc.)"]
    end

    subgraph BUFFERS["🔄 Ring Buffers (thread-safe)"]
        RB1["portMUX spinlock\nbuffer USB"]
        RB2["portMUX spinlock\nbuffer BLE"]
        RB3["portMUX spinlock\nbuffers WiFi"]
    end

    USB_TASK --> RB1
    BLE_STACK --> RB2
    WIFI --> RB3
    RB1 --> TASK
    RB2 --> TASK
    RB3 --> TASK
    TASK --> LOOP
    LOOP --> USER

    style CORE0 fill:#1A237E,color:#fff,stroke:#283593
    style CORE1 fill:#1B5E20,color:#fff,stroke:#2E7D32
    style BUFFERS fill:#37474F,color:#fff,stroke:#546E7A
```

### Fluxo de um Evento NoteOn

```mermaid
sequenceDiagram
    participant USB as 🔌 Teclado USB
    participant DRIVER as USB Host Driver
    participant BUF as Ring Buffer
    participant HANDLER as MIDIHandler
    participant USER as Seu Código

    USB->>DRIVER: Pacote USB-MIDI (4 bytes)
    DRIVER->>BUF: Armazena com portMUX (Core 0)
    loop Cada loop()
        USER->>HANDLER: midiHandler.task()
        HANDLER->>BUF: Lê mensagens pendentes
        BUF->>HANDLER: [0x09, 0x90, 0x3C, 0x64]
        HANDLER->>HANDLER: Parseia → MIDIEventData
        Note over HANDLER: status="NoteOn"<br/>note=60 (C4)<br/>velocity=100
        HANDLER->>USER: getQueue() retorna evento
    end
```

---

## Camadas da Biblioteca

| Camada | Arquivo | Responsabilidade |
|--------|---------|-----------------|
| Feature detection | `ESP32_Host_MIDI.h` | Detecta USB, BLE, PSRAM por chip |
| Transporte abstrato | `MIDITransport.h` | Interface comum para todos transportes |
| Processador central | `MIDIHandler.h/.cpp` | Fila, accordes, notas ativas, envio |
| Configuração | `MIDIHandlerConfig.h` | Struct de configuração do handler |
| Transportes built-in | `USBConnection`, `BLEConnection`, `ESPNowConnection` | Registrados automaticamente |
| Transportes externos | `UART`, `RTP-MIDI`, `Ethernet`, `OSC`, `USBDevice` | Incluídos manualmente no sketch |
| Integração teoria | `GingoAdapter.h` | Bridge com Gingoduino |

---

## Casos de Uso Típicos

### Hub MIDI de Palco

```
Teclado USB ──────────────────────────────────────────┐
iPhone BLE ───────────────────────────────────────────┤
ESP-NOW (pedais) ─────────────────────────────────────┤──► MIDIHandler ──► USB Device → Computador FOH
                                                       │              ──► DIN-5 → Rack de efeitos
                                                       │              ──► ESP-NOW → Outros performers
```

### Interface de Estúdio

```
macOS (RTP-MIDI via WiFi) ────────────────────────────┐
Sintetizador DIN-5 ───────────────────────────────────┤──► MIDIHandler ──► USB Device → DAW
iPad (BLE MIDI) ──────────────────────────────────────┘              ──► DIN-5 (THRU) → Outros synths
```

---

## Próximos Passos

- [Instalação →](instalacao.md) — instalar via Arduino IDE ou PlatformIO
- [Primeiros Passos →](primeiros-passos.md) — primeiro sketch funcionando em 5 minutos
- [Configuração →](configuracao.md) — `MIDIHandlerConfig` e opções avançadas
