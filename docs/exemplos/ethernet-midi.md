# 🔗 Ethernet MIDI

O exemplo `Ethernet-MIDI` implementa um hub MIDI sobre Ethernet cabeada (W5500 SPI), usando o protocolo AppleMIDI (RTP-MIDI). Ideal para racks de estúdio e instalações com rede gerenciada.

---

## Hardware Necessário

| Componente | Detalhe |
|-----------|---------|
| ESP32 (qualquer) | ESP32, S3, S2, P4... |
| Módulo W5500 | SPI Ethernet (ex: W5500 Mini) |
| Cabo | RJ-45 Ethernet para switch/router |

---

## Conexão W5500 ↔ ESP32

| W5500 | ESP32 GPIO | Notas |
|-------|-----------|-------|
| MOSI | 23 | SPI MOSI |
| MISO | 19 | SPI MISO |
| SCK | 18 | SPI Clock |
| CS (SCS) | 5 | Chip Select |
| RST | 4 | Reset (opcional) |
| VCC | 3.3V | Alimentação |
| GND | GND | Terra |

!!! tip "W5500 com ESP32-S3"
    Para ESP32-S3, use os pinos SPI2: MOSI=11, MISO=13, SCK=12, CS=10.

---

## Pré-requisito

```
Manage Libraries →
  "AppleMIDI" → Arduino-AppleMIDI-Library by lathoub (v3.x)
  "Ethernet" → Ethernet by Arduino (ou compatível com W5500)
```

---

## Código

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/EthernetMIDIConnection.h"

// MAC único para o dispositivo (escolha qualquer valor único na rede)
static const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Pinos SPI para W5500
#define ETH_CS_PIN 5

EthernetMIDIConnection ethMIDI;

void setup() {
    Serial.begin(115200);

    Serial.println("Iniciando Ethernet MIDI...");

    // Registrar ANTES de begin()
    midiHandler.addTransport(&ethMIDI);

    // DHCP automático
    ethMIDI.begin(MAC);

    // Ou IP estático:
    // ethMIDI.begin(MAC, IPAddress(192, 168, 1, 100));

    midiHandler.begin();

    Serial.printf("Ethernet IP: %s\n",
        Ethernet.localIP().toString().c_str());
    Serial.println("Configure Audio MIDI Setup no macOS:");
    Serial.printf("  Host: %s  Porta: 5004\n",
        Ethernet.localIP().toString().c_str());
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[ETH-MIDI] %s %s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.channel0 + 1,
            ev.velocity7);
    }
}
```

---

## Configurar no macOS

Como o W5500 não suporta mDNS, configure o IP manualmente:

1. **Audio MIDI Setup** → **Window → Show MIDI Studio**
2. Clique em **Network**
3. Em "My Sessions" → **+** → criar nova sessão
4. Em "Directory" → **+** → **Host: [IP do ESP32]** | **Port: 5004**
5. Selecione a sessão → **Connect**

---

## Bridge UART + Ethernet (Rack de Estúdio)

Conecte um sintetizador vintage via DIN-5 ao ESP32, que o publica na rede Ethernet para o DAW:

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"
#include "src/EthernetMIDIConnection.h"

UARTConnection uartMIDI;
EthernetMIDIConnection ethMIDI;
const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    uartMIDI.begin(Serial1, 16, 17);     // DIN-5
    ethMIDI.begin(MAC, IPAddress(192, 168, 1, 50));

    midiHandler.addTransport(&uartMIDI);
    midiHandler.addTransport(&ethMIDI);
    midiHandler.begin();

    // Bridge automático! Qualquer MIDI do DIN-5 vai para Ethernet e vice-versa.
}

void loop() { midiHandler.task(); }
```

---

## Latência Típica

| Ambiente | Latência | Jitter |
|---------|---------|--------|
| WiFi doméstico | 8–25 ms | ±5 ms |
| **Ethernet cabeada** | **2–8 ms** | **< 1 ms** |
| Ethernet gerenciada (QoS) | 1–3 ms | < 0.5 ms |

A Ethernet é mais previsível e consistente — fundamental para timing musical preciso em estúdio.

---

## Próximos Passos

- [Ethernet MIDI →](../transportes/ethernet-midi.md) — detalhes do transporte
- [UART / DIN-5 →](../transportes/uart-din5.md) — adicionar porta DIN-5
- [RTP-MIDI WiFi →](rtp-midi-wifi.md) — versão sem fio com mDNS
