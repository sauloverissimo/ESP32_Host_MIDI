# 🔗 Ethernet MIDI

Mesmo protocolo RTP-MIDI / AppleMIDI do WiFi, mas sobre **Ethernet cabeada**. Latência mais baixa e consistente — ideal para estúdios e instalações com rede gerenciada.

---

## Características

| Aspecto | Detalhe |
|---------|---------|
| Protocolo | AppleMIDI / RTP-MIDI (RFC 6295) |
| Físico | Ethernet cabeada (RJ-45) |
| Latência | 2–10 ms (mais estável que WiFi) |
| Hardware | W5500 SPI Ethernet **ou** ESP32-P4 (MAC nativo) |
| Requer | `lathoub/Arduino-AppleMIDI-Library` + `arduino-libraries/Ethernet` |

---

## Hardware

### Opção 1: Módulo W5500 SPI (qualquer ESP32)

O W5500 é um chip Ethernet completo com interface SPI. Funciona em **qualquer ESP32**.

```
Módulo W5500
  MOSI  → ESP32 GPIO 23
  MISO  → ESP32 GPIO 19
  SCK   → ESP32 GPIO 18
  CS    → ESP32 GPIO 5
  RST   → ESP32 GPIO 4
  VCC   → 3.3V
  GND   → GND
```

### Opção 2: ESP32-P4 (MAC Ethernet nativo)

O ESP32-P4 tem um controlador Ethernet MAC nativo (IEEE 802.3). Requer apenas um PHY externo (ex: LAN8720):

```cpp
#if ESP32_HOST_MIDI_HAS_ETH_MAC
    // MAC nativo disponível — configure o PHY no sdkconfig
#endif
```

---

## Código

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/EthernetMIDIConnection.h"  // Requer AppleMIDI + Ethernet

EthernetMIDIConnection ethMIDI;
static const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    Serial.begin(115200);

    // Registrar antes de begin()
    midiHandler.addTransport(&ethMIDI);

    // DHCP automático
    ethMIDI.begin(MAC);  // (1)

    // Ou IP estático:
    // ethMIDI.begin(MAC, IPAddress(192, 168, 1, 100));  // (2)

    midiHandler.begin();

    Serial.printf("Ethernet MIDI: %s\n",
        Ethernet.localIP().toString().c_str());
}

void loop() {
    midiHandler.task();

    for (const auto& ev : midiHandler.getQueue()) {
        Serial.printf("[ETH] %s %s vel=%d\n",
            ev.status.c_str(),
            ev.noteOctave.c_str(),
            ev.velocity);
    }
}
```

**Anotações:**

1. DHCP: O módulo W5500 obtém IP automaticamente via DHCP
2. IP estático: Passe um `IPAddress` como segundo argumento

---

## Conectar no macOS

!!! note "mDNS não disponível no Ethernet com W5500"
    A biblioteca Ethernet padrão não suporta mDNS/Bonjour. Você deve configurar o IP manualmente no Audio MIDI Setup.

### Passo a passo

1. Abra **Audio MIDI Setup** → **Window → Show MIDI Studio**
2. Clique em **Network**
3. Em "My Sessions", clique em **+** para criar uma sessão
4. Em "Directory" → **+** → Digite o IP do ESP32 e porta **5004**
5. Selecione a sessão e clique em **Connect**

---

## WiFi vs. Ethernet — Comparação

| Aspecto | WiFi (RTP-MIDI) | Ethernet (W5500) |
|---------|----------------|-----------------|
| Cabo | Sem fio | RJ-45 |
| Latência média | 5–20 ms | **2–10 ms** |
| Jitter | Moderado | **Muito baixo** |
| mDNS / Auto-discovery | ✅ | ❌ (IP manual) |
| Chips compatíveis | Qualquer com WiFi | **Qualquer ESP32** |
| Ideal para | Uso casual / palco | **Estúdio / instalação** |

---

## Exemplo Prático — Rack de Estúdio

```
Sintetizador DIN-5 ──► ESP32 (UART) ──► Ethernet W5500 ──► Switch ──► macOS Logic Pro
```

```cpp
#include <SPI.h>
#include <ESP32_Host_MIDI.h>
#include "src/UARTConnection.h"
#include "src/EthernetMIDIConnection.h"

UARTConnection uartMIDI;
EthernetMIDIConnection ethMIDI;
const uint8_t MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    uartMIDI.begin(Serial1, 16, 17);
    ethMIDI.begin(MAC, IPAddress(192, 168, 1, 50));

    midiHandler.addTransport(&uartMIDI);
    midiHandler.addTransport(&ethMIDI);
    midiHandler.begin();
    // Bridge automático DIN-5 ↔ Ethernet!
}

void loop() {
    midiHandler.task();
}
```

---

## Exemplos

| Exemplo | Descrição |
|---------|-----------|
| `Ethernet-MIDI` | Ethernet MIDI básico com W5500 |

---

## Próximos Passos

- [RTP-MIDI WiFi →](rtp-midi.md) — alternativa sem fio com mDNS
- [UART / DIN-5 →](uart-din5.md) — adicionar porta DIN-5 ao rack
- [Hardware Suportado →](../avancado/hardware.md) — ESP32-P4 com Ethernet nativo
