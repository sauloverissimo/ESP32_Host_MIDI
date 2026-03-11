# 🎨 OSC Bridge

O exemplo `T-Display-S3-OSC` implementa uma bridge bidirecional OSC ↔ MIDI: eventos do teclado USB são enviados como mensagens OSC para o computador, e mensagens OSC do computador são convertidas em MIDI e exibidas no display.

---

## Hardware Necessário

| Componente | Detalhe |
|-----------|---------|
| Placa | LilyGO T-Display-S3 |
| Teclado | Qualquer USB MIDI class-compliant |
| Cabo | USB-OTG |
| Rede | WiFi (mesma rede que o computador) |

---

## Pré-requisito

```
Manage Libraries → "OSC" → OSC by Adrian Freed, Yotam Mann (CNMAT)
```

---

## Código Completo

```cpp
#include <WiFi.h>
#include <ESP32_Host_MIDI.h>
#include "src/OSCConnection.h"
// Tools > USB Mode → "USB Host"

const char* WIFI_SSID       = "SeuSSID";
const char* WIFI_PASSWORD   = "SuaSenha";
IPAddress   REMOTE_IP       = IPAddress(192, 168, 1, 100);  // IP do computador (Max/MSP)
const int   LOCAL_OSC_PORT  = 8000;   // ESP32 escuta aqui
const int   REMOTE_OSC_PORT = 9000;   // Max/MSP escuta aqui

OSCConnection oscMIDI;

void setup() {
    Serial.begin(115200);

    Serial.print("Conectando WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Enviando OSC para: %s:%d\n",
        REMOTE_IP.toString().c_str(), REMOTE_OSC_PORT);
    Serial.printf("Recebendo OSC em: :%d\n", LOCAL_OSC_PORT);

    oscMIDI.begin(LOCAL_OSC_PORT, REMOTE_IP, REMOTE_OSC_PORT);
    midiHandler.addTransport(&oscMIDI);
    midiHandler.begin();

    Serial.println("Bridge OSC ↔ MIDI pronta!");
}

void loop() {
    midiHandler.task();

    // Eventos MIDI (de USB ou OSC recebido do computador)
    for (const auto& ev : midiHandler.getQueue()) {
        char noteBuf[8];
        Serial.printf("[MIDI] %s %s ch=%d vel=%d\n",
            MIDIHandler::statusName(ev.statusCode),
            MIDIHandler::noteWithOctave(ev.noteNumber, noteBuf, sizeof(noteBuf)),
            ev.channel0 + 1,
            ev.velocity7);

        // USB → OSC é automático (oscMIDI.sendMidiMessage() é chamado internamente)
    }
}
```

---

## Endereços OSC

| Direção | Endereço OSC | Argumentos |
|---------|-------------|-----------|
| ESP32 → Computador | `/midi/noteon` | `int channel, int note, int velocity` |
| ESP32 → Computador | `/midi/noteoff` | `int channel, int note, int velocity` |
| ESP32 → Computador | `/midi/cc` | `int channel, int controller, int value` |
| ESP32 → Computador | `/midi/pitchbend` | `int channel, int bend` |
| Computador → ESP32 | `/midi/noteon` | `int channel, int note, int velocity` |
| Computador → ESP32 | (todos os acima) | — |

---

## Patch Max/MSP

Para receber MIDI do ESP32 no Max/MSP:

```max
[udpreceive 9000]
      |
  [oscparse]
      |
  [route /midi/noteon /midi/noteoff /midi/cc]
  |              |              |
[unpack i i i] [unpack i i i] [unpack i i i]
  |    |   |
 ch  note vel
```

Para enviar MIDI do Max/MSP para o ESP32:

```max
[pack i i i]       ← channel note velocity
      |
[oscformat /midi/noteon]
      |
[udpsend]
[connect 192.168.1.X 8000]   ← IP do ESP32
```

---

## Patch Pure Data

**Receber:**
```
[udpreceive 9000]
      |
  [import osc]
  [oscparse]
      |
  [route /midi/noteon]
```

**Enviar:**
```
[pack f f f]
      |
[oscformat /midi/noteon]
      |
[udpsend 192.168.1.X 8000]
```

---

## TouchOSC

Configure o TouchOSC (iOS/Android):

1. Em **Connections → OSC**:
   - Host: IP do ESP32
   - Port (outgoing): 8000
   - Port (incoming): 9000
2. Crie botões com endereços `/midi/noteon` e argumentos: canal, nota, velocidade

---

## Próximos Passos

- [OSC →](../transportes/osc.md) — detalhes do transporte OSC
- [RTP-MIDI →](rtp-midi-wifi.md) — alternativa para DAWs com AppleMIDI
