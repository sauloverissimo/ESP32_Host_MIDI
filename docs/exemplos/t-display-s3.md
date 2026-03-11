# 🖥️ T-Display-S3 Piano

O exemplo `T-Display-S3-Piano` transforma a LilyGO T-Display-S3 em um visualizador de piano ao vivo com rolagem de 25 teclas, detecção de acordes e exibição de informações MIDI em tempo real.

---

## Hardware Necessário

| Componente | Modelo |
|-----------|-------|
| Placa | LilyGO T-Display-S3 |
| Display | ST7789 1.9" 170×320 (embutido) |
| Teclado | Qualquer USB MIDI class-compliant |
| Cabo | USB-OTG (micro para USB-A fêmea) |

---

## O Que o Exemplo Faz

- **Piano roll de 25 teclas**: mostra as teclas C4 a C6 como um piano horizontal
- **Rolagem automática**: a janela de 25 teclas rola para mostrar as notas ativas
- **Notas ativas**: teclas pressionadas aparecem em destaque
- **Log de eventos**: últimos 8 eventos na tela (nota, canal, velocidade, timestamp)
- **Informações USB**: status da conexão USB Host

---

## Configuração Arduino IDE

```
Board: "LilyGo T-Display-S3" (ou "ESP32S3 Dev Module")
Tools > USB Mode → "USB Host"
Tools > PSRAM → "OPI PSRAM"
Upload Speed: 921600
```

---

## Estrutura do Exemplo

```
examples/T-Display-S3-Piano/
├── T-Display-S3-Piano.ino    ← Sketch principal
├── mapping.h                  ← Pinos de hardware
├── ST7789_Handler.h           ← Interface do display
└── ST7789_Handler.cpp         ← Implementação do display
```

---

## mapping.h — Pinos

```cpp
// T-Display-S3 pinout
#define TFT_CS    6
#define TFT_RST   5
#define TFT_DC    7
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_BL    38   // Backlight
```

---

## Sketch Principal (simplificado)

```cpp
#include <ESP32_Host_MIDI.h>
#include "src/GingoAdapter.h"   // Detecção de acordes
#include "ST7789_Handler.h"
#include "mapping.h"
// Tools > USB Mode → "USB Host"

ST7789_Handler display;

void setup() {
    Serial.begin(115200);
    display.begin();
    display.clear();

    MIDIHandlerConfig cfg;
    cfg.maxEvents = 20;
    cfg.chordTimeWindow = 50;
    midiHandler.begin(cfg);
}

void loop() {
    midiHandler.task();

    // Atualizar display quando notas mudarem
    bool active[128] = {false};
    midiHandler.fillActiveNotes(active);
    display.updatePianoRoll(active);

    // Mostrar nome do acorde
    char chord[16] = "";
    if (midiHandler.getActiveNotesCount() > 0) {
        GingoAdapter::identifyLastChord(midiHandler, chord, sizeof(chord));
    }
    display.showChord(chord);

    // Log de eventos
    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        display.addEvent(MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)), ev.channel0 + 1, ev.velocity7);
    }

    display.render();
}
```

---

## Galeria

<div style="display:flex; gap:12px; flex-wrap:wrap; justify-content:center; margin:20px 0">
  <figure style="margin:0; text-align:center">
    <img src="https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/examples/T-Display-S3-Piano/images/pianno-MIDI-25.jpeg" width="300" alt="Piano Roll" style="border-radius:8px"/>
    <figcaption><em>Piano roll de 25 teclas com notas ativas</em></figcaption>
  </figure>
  <figure style="margin:0; text-align:center">
    <img src="https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/examples/T-Display-S3-Gingoduino/images/gingo-duino-integration.jpeg" width="300" alt="Chord Names" style="border-radius:8px"/>
    <figcaption><em>Detecção de acordes em tempo real</em></figcaption>
  </figure>
</div>

---

## Outros Exemplos T-Display-S3

| Exemplo | Transporte | O que mostra |
|---------|-----------|-------------|
| `T-Display-S3` | USB Host | Notas ativas + log |
| `T-Display-S3-Queue` | USB Host | Fila de eventos debug |
| `T-Display-S3-Piano` | USB Host | Piano roll 25 teclas |
| `T-Display-S3-Piano-Debug` | USB Host | Piano + debug estendido |
| `T-Display-S3-Gingoduino` | USB + BLE | Acordes (Gingoduino) |
| `T-Display-S3-BLE-Sender` | BLE | Sequenciador BLE |
| `T-Display-S3-BLE-Receiver` | BLE | Receptor BLE |
| `T-Display-S3-OSC` | OSC + WiFi | Bridge OSC |
| `T-Display-S3-USB-Device` | BLE + USB Device | Bridge duplo |
| `T-Display-S3-ESP-NOW-Jam` | ESP-NOW | Jam colaborativo |

---

## Próximos Passos

- [GingoAdapter →](../funcionalidades/gingo-adapter.md) — detecção de acordes
- [USB Host →](../transportes/usb-host.md) — detalhes do transporte USB
- [UART Básico →](uart-basico.md) — exemplo mais simples
