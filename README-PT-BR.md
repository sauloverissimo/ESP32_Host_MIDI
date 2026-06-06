# ESP32\_Host\_MIDI

## O hub MIDI universal para ESP32. Uma única API de eventos para USB, BLE, WiFi, Ethernet, ESP-NOW, OSC e DIN-5.

![ESP32_Host_MIDI](https://raw.githubusercontent.com/sauloverissimo/ESP32_Host_MIDI/main/ESP32_Host_MIDI.png)

*Biblioteca Arduino, multi-transporte, MIDI 1.0 e 2.0, MIT.* Oito transportes, uma fila de eventos.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![CI](https://github.com/sauloverissimo/ESP32_Host_MIDI/actions/workflows/ci.yml/badge.svg)](https://github.com/sauloverissimo/ESP32_Host_MIDI/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/sauloverissimo/ESP32_Host_MIDI)](https://github.com/sauloverissimo/ESP32_Host_MIDI/releases)
[![Platform](https://img.shields.io/badge/Platform-Arduino%20%7C%20PlatformIO%20%7C%20ESP--IDF-00979D.svg)](#instalação)
[![MIDI](https://img.shields.io/badge/MIDI-1.0%20%7C%202.0-blueviolet.svg)](https://midi.org/specifications)
[![Sponsor](https://img.shields.io/badge/sponsor-%E2%9D%A4-pink.svg)](https://github.com/sponsors/sauloverissimo)

**Idioma:** [🇺🇸 English](README.md)

---

## Visão geral

O ESP32\_Host\_MIDI transforma um ESP32 em um hub MIDI multi-protocolo. Conecte um teclado USB MIDI pelo USB Host, receba notas de um iPhone por BLE, conecte um DAW pelo WiFi com RTP-MIDI (Apple MIDI), fale com o Max/MSP por OSC, alcance equipamentos DIN-5 antigos pela serial e ligue vários ESP32 por ESP-NOW. Cada transporte entrega em uma única fila de eventos do `MIDIHandler` e compartilha uma só API de envio.

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;

void setup() {
    Serial.begin(115200);
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}

void loop() {
    midiHandler.task();
    for (const auto& ev : midiHandler.getQueue()) {
        char buf[8];
        Serial.printf("%-12s %-4s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, buf, sizeof(buf)),
            ev.channel0 + 1, ev.velocity7);
    }
}
```

---

## Transportes

| Transporte | Protocolo | Física | Latência | Requer |
|-----------|----------|--------|----------|--------|
| [USB Host](#usb-host) | USB MIDI 1.0 | Cabo USB-OTG | < 1 ms | ESP32-S3 / S2 / P4 |
| [USB Host MIDI 2.0](#usb-host-midi-20) | USB MIDI 2.0 (UMP) | Cabo USB-OTG | < 1 ms | ESP32-S3 / S2 / P4 |
| [BLE MIDI](#ble-midi) | BLE MIDI 1.0 | Bluetooth LE | 3-15 ms | Qualquer ESP32 com BT |
| [ESP-NOW](#esp-now) | ESP-NOW | Rádio 2,4 GHz | 1-5 ms | Qualquer ESP32 |
| [RTP-MIDI](#rtp-midi-apple-midi) | AppleMIDI / RFC 6295 | UDP WiFi | 5-20 ms | Qualquer ESP32 com WiFi |
| [Ethernet MIDI](#ethernet-midi) | AppleMIDI / RFC 6295 | Cabeado (W5500 / nativo) | 2-10 ms | W5500 SPI ou ESP32-P4 |
| [OSC](#osc) | Open Sound Control | UDP WiFi | 5-15 ms | Qualquer ESP32 com WiFi |
| [UART / DIN-5](#uart--din-5) | Serial MIDI 1.0 | Conector DIN-5 | < 1 ms | Qualquer ESP32 |

Todo transporte implementa a mesma interface `MIDITransport` e se registra com uma linha: `midiHandler.addTransport(&t)`.

---

## Início rápido

Desde a v6.0 os transportes são explícitos: inclua o header que você usa, declare o transporte, registre com `addTransport()` e chame o `begin()` dele. Veja [`docs/migration-v6.md`](docs/migration-v6.md) se estiver migrando da v5.x.

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>   // inclua apenas os transportes que você usa
// Arduino IDE: Tools > USB Mode > "USB Host"

USBConnection usbHost;

void setup() {
    Serial.begin(115200);
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}

void loop() {
    midiHandler.task();
    for (const auto& ev : midiHandler.getQueue()) {
        char buf[8];
        Serial.println(MIDIHandler::noteWithOctave(ev.noteNumber, buf, sizeof(buf)));
    }
}
```

### Lendo eventos

Cada `MIDIEventData` na fila expõe a resolução MIDI 1.0 e MIDI 2.0:

```cpp
for (const auto& ev : midiHandler.getQueue()) {
    ev.statusCode;   // MIDI_NOTE_ON | MIDI_NOTE_OFF | MIDI_CONTROL_CHANGE | ...
    ev.channel0;     // 0-15 (convenção da spec MIDI)
    ev.noteNumber;   // 0-127 (número do controlador no CC)
    ev.velocity7;    // 0-127 (MIDI 1.0)
    ev.velocity16;   // 0-65535 (MIDI 2.0, escalado)
    ev.pitchBend14;  // 0-16383 (centro = 8192)
    ev.pitchBend32;  // 0-0xFFFFFFFF (MIDI 2.0, centro = 0x80000000)
    ev.chordIndex;   // agrupa notas simultâneas
    ev.timestamp;    // millis() na chegada

    // Helpers estáticos (zero alocação):
    MIDIHandler::noteName(ev.noteNumber);    // "C", "C#", "D" ...
    MIDIHandler::noteOctave(ev.noteNumber);  // -1 a 9
    MIDIHandler::statusName(ev.statusCode);  // "NoteOn", "ControlChange" ...
}
```

---

## Envio e pontes

Envie por qualquer transporte com a API unificada. O `send*` tenta cada transporte registrado em ordem e para no primeiro que aceitar a mensagem (first-wins). O canal vai de 1 a 16.

```cpp
midiHandler.sendNoteOn(1, 60, 100);
midiHandler.sendControlChange(1, 64, 127);
midiHandler.sendPitchBend(1, 0);           // -8192 a +8191, centro = 0
```

O MIDI **não** é roteado automaticamente entre transportes. Cada transporte entrega o MIDI recebido na fila de eventos compartilhada; o seu `loop()` decide o que encaminhar. Para fazer ponte entre dois transportes, leia a fila e reenvie ao transporte de destino:

```cpp
// Encaminha note/CC de qualquer entrada para um transporte de destino.
static int lastIndex = 0;
for (const auto& ev : midiHandler.getQueue()) {
    if (ev.index <= lastIndex) continue;
    lastIndex = ev.index;
    uint8_t msg[3] = { uint8_t(ev.statusCode | ev.channel0), ev.noteNumber, ev.velocity7 };
    target.sendMidiMessage(msg, 3);
}
```

| Ponte | Caminho |
|-------|---------|
| Teclado USB para WiFi | Teclado USB -> ESP32 -> RTP-MIDI -> macOS |
| Moderno para legado | macOS -> RTP-MIDI -> ESP32 -> DIN-5 -> caixa de ritmo dos anos 80 |
| Mesh de palco sem fio | Nós ESP-NOW -> hub ESP32 -> RTP-MIDI -> computador da FOH |
| Software criativo | Max/MSP OSC -> ESP32 -> BLE -> app instrumento no iPad |

---

## Referência dos transportes

### USB Host

Conecta qualquer dispositivo USB MIDI class-compliant (teclados, pads, interfaces, controladores) direto na porta USB-OTG do ESP32. Sem hub, sem driver, sem configuração de sistema.

**Placas:** ESP32-S3, S2, P4 · **Arduino IDE:** `Tools > USB Mode > "USB Host"`

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>

USBConnection usbHost;

void setup() {
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}
```

Para um exemplo completo de host que também decodifica MIDI 2.0, veja o exemplo `USB-Host-MIDI2`.

### USB Host MIDI 2.0

USB MIDI 2.0 / UMP nativo. O `USBMIDI2Connection` estende o `USBConnection`, varre o descriptor de configuração do dispositivo em busca de Alt 0 (MIDI 1.0) e Alt 1 (MIDI 2.0), e prefere MIDI 2.0 quando disponível, com fallback para MIDI 1.0. Após a negociação ele faz a discovery UMP somente leitura (Endpoint Info, Function Block Info). As palavras UMP de 32 bits chegam um pacote inteiro por vez, remontadas entre transferências USB.

```cpp
#include <USBMIDI2Connection.h>

USBMIDI2Connection usb;

void onUMP(void*, const uint32_t* words, uint8_t count) {
    // Palavras UMP cruas, resolução MIDI 2.0 nativa.
}

void setup() {
    usb.setUMPCallback(onUMP, nullptr);
    usb.setMidiCallback(onMidi, nullptr);   // fallback MIDI 1.0
    usb.begin();
}
```

Consulte as capacidades negociadas:

```cpp
if (usb.isMIDI2() && usb.isNegotiated()) {
    const auto& ep = usb.getEndpointInfo();
    Serial.printf("UMP v%d.%d, %d function blocks\n",
        ep.umpVersionMajor, ep.umpVersionMinor, ep.numFunctionBlocks);
}
```

**Placas:** ESP32-S3, S2, P4 · **Exemplos:** `USB-Host-MIDI2`, `T-Display-S3-Piano-Flow`

### BLE MIDI

O ESP32 anuncia como periférico BLE MIDI 1.0. macOS (**Audio MIDI Setup > Bluetooth**), iOS (GarageBand, AUM, Loopy, Moog) e Android conectam sem ritual de pareamento. O modo Central (scanner) conecta a outro dispositivo BLE MIDI.

**Placas:** Qualquer ESP32 com Bluetooth · **Alcance:** ~30 m · **Latência:** 3-15 ms

```cpp
#include <ESP32_Host_MIDI.h>
#include <BLEConnection.h>

BLEConnection ble;

void setup() {
    ble.begin("ESP32 MIDI");
    midiHandler.addTransport(&ble);
    midiHandler.begin();
}
```

**Exemplos:** `T-Display-S3-BLE-Sender`, `T-Display-S3-BLE-Receiver`

### ESP-NOW

MIDI sem fio de baixa latência entre placas ESP32 pelo rádio peer-to-peer da Espressif. Sem roteador WiFi, sem handshake, sem pareamento. Broadcast (toda placa ouve a todos) ou unicast.

**Placas:** Qualquer ESP32 · **Alcance:** ~200 m em campo aberto · **Infraestrutura:** nenhuma

```cpp
#include <ESP32_Host_MIDI.h>
#include <ESPNowConnection.h>

ESPNowConnection espNow;

void setup() {
    espNow.begin();
    midiHandler.addTransport(&espNow);
    midiHandler.begin();
}
```

**Exemplos:** `T-Display-S3-ESP-NOW-Jam`

### RTP-MIDI (Apple MIDI)

**Apple MIDI** (RTP-MIDI, RFC 6295) sobre UDP WiFi. macOS e iOS descobrem o ESP32 por **mDNS Bonjour** e o mostram em **Audio MIDI Setup > Network** sem configuração manual. Funciona com Logic Pro, GarageBand, Ableton e qualquer app CoreMIDI.

**Requer:** `lathoub/Arduino-AppleMIDI-Library` v3.x

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include <RTPMIDIConnection.h>

RTPMIDIConnection rtpMIDI;

void setup() {
    WiFi.begin("SuaRede", "SuaSenha");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    rtpMIDI.begin("ESP32 MIDI");
    midiHandler.addTransport(&rtpMIDI);
    midiHandler.begin();
}
```

**Exemplos:** `RTP-MIDI-WiFi`

### Ethernet MIDI

O mesmo protocolo RTP-MIDI / AppleMIDI sobre um módulo Ethernet SPI W5500 ou o MAC Ethernet nativo do ESP32-P4. Latência menor e mais constante que o WiFi. Ideal para racks de estúdio e palcos.

**Requer:** `lathoub/Arduino-AppleMIDI-Library` v3.x e a biblioteca `Ethernet` do Arduino

```cpp
#include <ESP32_Host_MIDI.h>
#include <EthernetMIDIConnection.h>

EthernetMIDIConnection ethMIDI;
static const uint8_t MAC[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
    ethMIDI.begin(MAC);   // DHCP; passe um IPAddress estático como segundo argumento para IP fixo
    midiHandler.addTransport(&ethMIDI);
    midiHandler.begin();
}
```

**Exemplos:** `Ethernet-MIDI`

### OSC

Ponte bidirecional **OSC para MIDI** sobre UDP WiFi. Recebe OSC do Max/MSP, Pure Data, SuperCollider e TouchOSC e converte em eventos MIDI, e envia cada evento MIDI como OSC.

**Mapa de endereços:** `/midi/noteon`, `/midi/noteoff`, `/midi/cc`, `/midi/pc`, `/midi/pitchbend`, `/midi/aftertouch`
**Requer:** biblioteca `CNMAT/OSC`

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include <OSCConnection.h>

OSCConnection oscMIDI;

void setup() {
    WiFi.begin("SuaRede", "SuaSenha");
    while (WiFi.status() != WL_CONNECTED) delay(500);
    oscMIDI.begin(8000, IPAddress(192, 168, 1, 100), 9000);
    midiHandler.addTransport(&oscMIDI);
    midiHandler.begin();
}
```

**Exemplos:** `T-Display-S3-OSC`

### UART / DIN-5

MIDI serial padrão (31250 baud, 8N1) para hardware vintage: sintetizadores, caixas de ritmo, mixers, sequenciadores, qualquer coisa com conector DIN-5. Suporta running status, mensagens de tempo real (Clock, Start, Stop) e várias portas UART (o ESP32-P4 tem cinco UARTs por hardware).

**Hardware:** TX para o pino 5 do DIN-5 via 220 Ohm; optoacoplador PC-900V / 6N138 no RX para o pino 4 do DIN-5

```cpp
#include <ESP32_Host_MIDI.h>
#include <UARTConnection.h>

UARTConnection uartMIDI;

void setup() {
    uartMIDI.begin(Serial1, /*RX=*/16, /*TX=*/17);
    midiHandler.addTransport(&uartMIDI);
    midiHandler.begin();
}
```

**Exemplos:** `UART-MIDI-Basic`, `P4-Dual-UART-MIDI`

---

## Arquitetura

```
ENTRADAS                            MIDIHandler              SAÍDAS

Teclado USB   --[USBConnection]------>  +--------------+
USB MIDI 2.0  --[USBMIDI2Connection]->  |              |
iPhone BLE    --[BLEConnection]------>  |  Fila de     |--> getQueue()
macOS WiFi    --[RTPMIDIConnection]-->  |  eventos     |
W5500 LAN     --[EthernetMIDIConn.]-->  |  (ring buf,  |--> Notas ativas
Max/MSP OSC   --[OSCConnection]------>  |  thread-safe)|
DIN-5 serial  --[UARTConnection]----->  |  Detecção de |--> Índice de
Rádio ESP32   --[ESPNowConnection]--->  +------+-------+    acorde
                                               |
                                               v
                                        send* / sendMidiMessage()
                                        (primeiro transporte que aceitar)
```

**Core 0** roda a task do USB Host, a pilha BLE e os drivers de rádio/rede (tarefas FreeRTOS).
**Core 1** roda `midiHandler.task()` e o seu `loop()`. Cada transporte usa ring buffers e spinlocks `portMUX` para segurança entre cores.

---

## Compatibilidade de hardware

| Chip | USB Host | BLE | WiFi | Ethernet (nativo) | UART | ESP-NOW |
|------|:--------:|:---:|:----:|:-----------------:|:----:|:-------:|
| ESP32-S3 | sim | sim | sim | W5500 SPI | sim | sim |
| ESP32-S2 | sim | não | sim | W5500 SPI | sim | não |
| ESP32-P4 | sim | não | não | sim | sim (x5) | não |
| ESP32 (clássico) | não | sim | sim | W5500 SPI | sim | sim |
| ESP32-C3 / C6 / H2 | não | sim | sim | não | sim | sim |

> O Ethernet SPI W5500 funciona em qualquer ESP32 através do `EthernetMIDIConnection`. A **LilyGO T-Display-S3** (ESP32-S3 + display de 1,9") é a melhor placa geral para USB Host, BLE, WiFi e um dashboard MIDI ao vivo.

---

## Exemplos

A pasta [`examples/`](examples) traz sketches prontos, vários com foto e demo em `.mp4` em `examples/<nome>/images/`.

| Exemplo | Transporte | O que mostra |
|---------|-----------|--------------|
| `USB-Host-MIDI2` | USB Host MIDI 2.0 | Recebe e decodifica UMP cru |
| `T-Display-S3-Piano-Flow` | USB Host MIDI 2.0 | Piano roll, acorde com inversão, duração da nota |
| `T-Display-S3-BLE-Sender` | BLE | Status do modo envio + log |
| `T-Display-S3-BLE-Receiver` | BLE | Modo recepção + log de notas |
| `T-Display-S3-ESP-NOW-Jam` | ESP-NOW | Status do par + eventos de jam |
| `RTP-MIDI-WiFi` | RTP-MIDI | Apple MIDI para macOS pelo WiFi |
| `Ethernet-MIDI` | Ethernet | Apple MIDI pelo W5500 / MAC nativo |
| `T-Display-S3-OSC` | OSC + WiFi | Ponte OSC para MIDI com display |
| `UART-MIDI-Basic` | UART / DIN-5 | DIN-5 entrada e saída |
| `P4-Dual-UART-MIDI` | UART / DIN-5 | Duas UARTs por hardware no ESP32-P4 |

<p align="center">
  <img src="examples/RTP-MIDI-WiFi/images/RTP.jpeg" width="280" />&nbsp;
  <img src="examples/T-Display-S3-BLE-Receiver/images/BLE.jpeg" width="280" />
</p>
<p align="center"><em>RTP-MIDI / Apple MIDI para macOS · BLE MIDI de um iPhone</em></p>

---

## Instalação

**Arduino IDE:** Sketch > Incluir Biblioteca > Gerenciar Bibliotecas, pesquise **ESP32_Host_MIDI**.

**PlatformIO:**

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board    = esp32-s3-devkitc-1
framework = arduino

lib_deps =
    sauloverissimo/ESP32_Host_MIDI
    # lathoub/Arduino-AppleMIDI-Library   ; RTP-MIDI + Ethernet MIDI
    # arduino-libraries/Ethernet          ; Ethernet MIDI
    # CNMAT/OSC                           ; OSC
```

**Pacote de placa:** `Tools > Boards Manager`, "esp32" por Espressif, versão >= 3.0.0. USB Host requer arduino-esp32 >= 3.0 (TinyUSB MIDI).

| Transporte | Biblioteca necessária |
|-----------|------------------------|
| RTP-MIDI / Ethernet MIDI | `lathoub/Arduino-AppleMIDI-Library` |
| Ethernet MIDI | `arduino-libraries/Ethernet` |
| OSC | `CNMAT/OSC` |
| USB Host / BLE / ESP-NOW / UART | já incluso no arduino-esp32 |

---

## Referência da API

```cpp
// Setup: cada transporte é explícito (v6.0+).
USBConnection usb;                 // #include <USBConnection.h>, <BLEConnection.h>, ...
BLEConnection ble;
ble.begin("Meu Device");           // o usuário controla o ciclo de vida de cada transporte
usb.begin();
midiHandler.addTransport(&usb);    // registra cada transporte
midiHandler.addTransport(&ble);
midiHandler.begin();               // defaults
midiHandler.begin(cfg);            // ou com um MIDIHandlerConfig custom
midiHandler.task();                // chamar a cada loop()

// Receber
const auto& q = midiHandler.getQueue();                          // ring buffer de eventos
std::vector<std::string> n = midiHandler.getActiveNotesVector(); // ["C4","E4","G4"]
size_t count = midiHandler.getActiveNotesCount();
// SysEx: midiHandler.getSysExQueue(), setSysExCallback(cb), sendSysEx(data, len)

// Enviar (vence o primeiro transporte que aceitar a mensagem)
midiHandler.sendNoteOn(ch, note, vel);        // ch: 1-16
midiHandler.sendNoteOff(ch, note, vel);
midiHandler.sendControlChange(ch, ctrl, val);
midiHandler.sendProgramChange(ch, prog);
midiHandler.sendPitchBend(ch, val);           // -8192..+8191, centro = 0
```

**MIDIHandlerConfig:**

```cpp
MIDIHandlerConfig cfg;
cfg.maxEvents         = 20;    // capacidade da fila (1..100)
cfg.chordTimeWindow   = 0;     // ms de agrupamento para detecção de acordes (0 = legado)
cfg.velocityThreshold = 0;     // ignora NoteOn abaixo desta velocity (0..127)
cfg.historyCapacity   = 0;     // buffer de histórico em PSRAM (0 = desabilitado)
cfg.maxSysExSize      = 512;   // bytes por SysEx (0 = desabilita SysEx)
cfg.maxSysExEvents    = 8;     // profundidade da fila de SysEx
midiHandler.begin(cfg);
```

**Transporte custom:** herde de `MIDITransport`, implemente `task()` e `isConnected()`, opcionalmente `sendMidiMessage()`, e chame o `dispatchMidiData()` herdado para injetar o MIDI recebido.

```cpp
class MeuTransporte : public MIDITransport {
    void task() override { /* lê o hardware, depois dispatchMidiData(bytes, len) */ }
    bool isConnected() const override { return connected; }
    bool sendMidiMessage(const uint8_t* data, size_t len) override { /* ... */ return true; }
};
midiHandler.addTransport(&meuTransporte);
```

---

## Licença

MIT, veja [LICENSE](LICENSE).

<p align="center">
  Feito para músicos, makers e pesquisadores.<br/>
  Issues e contribuições são bem-vindos em
  <a href="https://github.com/sauloverissimo/ESP32_Host_MIDI">github.com/sauloverissimo/ESP32_Host_MIDI</a>
</p>
