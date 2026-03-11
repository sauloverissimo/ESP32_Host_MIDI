# 🌐 RTP-MIDI WiFi

O exemplo `RTP-MIDI-WiFi` implementa um sequenciador de etapas (*step sequencer*) que envia notas via Apple MIDI (RTP-MIDI) para macOS — aparecendo automaticamente em Audio MIDI Setup.

---

## Hardware Necessário

| Componente | Detalhe |
|-----------|---------|
| Placa | LilyGO T-Display-S3 (ou qualquer ESP32 com WiFi) |
| Display | ST7789 (embutido no T-Display-S3) |
| Rede | WiFi 2,4 GHz ou 5 GHz |

---

## Pré-requisito

Instalar a biblioteca AppleMIDI:

```
Manage Libraries → "AppleMIDI" → Arduino-AppleMIDI-Library by lathoub (v3.x)
```

---

## Código Completo

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "src/RTPMIDIConnection.h"

// ─── Configuração ────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "SeuSSID";
const char* WIFI_PASSWORD = "SuaSenha";
const char* MIDI_NAME     = "Sequenciador ESP32";
const int   BPM           = 120;

// ─── Sequência ────────────────────────────────────────────────────────────────
const uint8_t NOTAS[]    = {60, 62, 64, 67, 69, 67, 64, 62};  // C D E G A G E D
const uint8_t VELS[]     = {100, 80, 90, 110, 95, 85, 75, 100};
const int     NUM_STEPS  = 8;

RTPMIDIConnection rtpMIDI;
int step = 0;
unsigned long nextTick = 0;
const unsigned long NOTE_MS = 60000UL / BPM / 2;  // colcheia

void setup() {
    Serial.begin(115200);

    // Conectar WiFi
    Serial.printf("Conectando a %s...", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nConectado! IP: %s\n", WiFi.localIP().toString().c_str());

    // Iniciar RTP-MIDI
    rtpMIDI.begin(MIDI_NAME);
    midiHandler.addTransport(&rtpMIDI);
    midiHandler.begin();

    Serial.printf("RTP-MIDI pronto — nome: '%s'\n", MIDI_NAME);
    Serial.println("Abra 'Audio MIDI Setup' no Mac e conecte à sessão.");
    nextTick = millis();
}

void loop() {
    midiHandler.task();

    // Receber MIDI do macOS (bidirecional)
    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[RTP IN] %s %s vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.velocity7);
    }

    // Sequenciador — tocar próximo step
    unsigned long now = millis();
    if (now >= nextTick) {
        // Desligar nota anterior
        uint8_t prevNote = NOTAS[(step + NUM_STEPS - 1) % NUM_STEPS];
        midiHandler.sendNoteOff(1, prevNote, 0);

        // Ligar próxima nota
        midiHandler.sendNoteOn(1, NOTAS[step], VELS[step]);
        Serial.printf("[SEQ] Step %d: %d (vel=%d)\n",
            step, NOTAS[step], VELS[step]);

        step = (step + 1) % NUM_STEPS;
        nextTick = now + NOTE_MS;
    }
}
```

---

## Configuração no macOS

```mermaid
sequenceDiagram
    participant ESP32 as ESP32 (Sequenciador)
    participant BONJOUR as mDNS / Bonjour
    participant MAC as macOS (Audio MIDI Setup)
    participant LOGIC as Logic Pro

    ESP32->>BONJOUR: Anuncia "Sequenciador ESP32"
    MAC->>BONJOUR: Descobre sessões na rede
    BONJOUR-->>MAC: "Sequenciador ESP32" disponível
    MAC->>ESP32: Connect
    MAC->>LOGIC: Porta "Sequenciador ESP32" disponível

    loop Sequenciador rodando
        ESP32->>MAC: NoteOn / NoteOff via RTP-MIDI
        MAC->>LOGIC: MIDI events na faixa
    end
```

### Passo a passo

1. Abra **Audio MIDI Setup** → **Window → Show MIDI Studio**
2. Clique no ícone **Network** (globo)
3. No painel "Directory", clique em **+** e selecione "Sequenciador ESP32"
4. Clique em **Connect**
5. Abra **Logic Pro** / **GarageBand** → a porta aparece automaticamente

---

## Galeria

<div style="display:flex; gap:12px; flex-wrap:wrap; justify-content:center; margin:20px 0">
  <figure style="margin:0; text-align:center">
    <img src="https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/examples/RTP-MIDI-WiFi/images/RTP.jpeg" width="300" alt="RTP-MIDI" style="border-radius:8px"/>
    <figcaption><em>Audio MIDI Setup — sessão ativa</em></figcaption>
  </figure>
  <figure style="margin:0; text-align:center">
    <img src="https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/examples/RTP-MIDI-WiFi/images/RTP2.jpeg" width="300" alt="RTP-MIDI macOS" style="border-radius:8px"/>
    <figcaption><em>Sequenciador conectado ao macOS</em></figcaption>
  </figure>
</div>

---

## Próximos Passos

- [RTP-MIDI →](../transportes/rtp-midi.md) — detalhes do transporte
- [Ethernet MIDI →](ethernet-midi.md) — versão cabeada com menor latência
- [OSC Bridge →](osc-bridge.md) — alternativa para Max/MSP
