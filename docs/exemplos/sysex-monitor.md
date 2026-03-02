# 🔧 T-Display-S3 SysEx Monitor

O exemplo `T-Display-S3-SysEx` transforma a LilyGO T-Display-S3 em um monitor SysEx com envio de Identity Request e visualização de eventos MIDI em tempo real.

---

## Hardware Necessário

| Componente | Modelo |
|-----------|-------|
| Placa | LilyGO T-Display-S3 |
| Display | ST7789 1.9" 170×320 (embutido) |
| Teclado/Controller | Qualquer USB MIDI class-compliant |
| Cabo | USB-OTG (micro para USB-A fêmea) |

---

## O Que o Exemplo Faz

- **Monitor SysEx**: exibe mensagens SysEx recebidas com identificação do tipo (Identity Request/Reply, Universal RT/NRT, Manufacturer-specific)
- **Visualização hex**: mostra os bytes da mensagem em hexadecimal, com truncamento inteligente para mensagens longas
- **Envio de Identity Request**: BTN1 (GPIO0) envia `F0 7E 7F 06 01 F7` para identificar o dispositivo conectado
- **Log de eventos MIDI**: mostra os últimos eventos de canal (NoteOn, NoteOff, CC, etc.)
- **Contadores**: SysEx na fila, eventos MIDI, notas ativas

---

## Controles

| Botão | GPIO | Função |
|-------|------|--------|
| BTN1 | 0 | Enviar Identity Request |
| BTN2 | 14 | Limpar filas (SysEx + MIDI) |

---

## Configuração Arduino IDE

```
Board:           ESP32S3 Dev Module
USB Mode:        USB Host (USB-OTG)
USB CDC on Boot: Enabled
PSRAM:           OPI PSRAM
Flash Size:      16MB (ou o tamanho da sua board)
Partition:       Huge APP (3MB)
```

---

## Código-Fonte

O exemplo completo está em [`examples/T-Display-S3-SysEx/`](https://github.com/sauloverissimo/ESP32_Host_MIDI/tree/main/examples/T-Display-S3-SysEx)

O ponto central é a configuração do SysEx:

```cpp
MIDIHandlerConfig config;
config.maxSysExSize = 256;    // máximo de bytes por mensagem
config.maxSysExEvents = 8;    // quantas mensagens na fila
midiHandler.begin(config);
```

E a leitura da fila no loop:

```cpp
const auto& sysexQueue = midiHandler.getSysExQueue();
for (auto it = sysexQueue.rbegin(); it != sysexQueue.rend(); ++it) {
    // it->index, it->timestamp, it->data
}
```

---

## Resultado

![T-Display-S3 SysEx Monitor](https://github.com/sauloverissimo/ESP32_Host_MIDI/raw/main/examples/T-Display-S3-SysEx/images/sysex.jpeg)

O display mostra o header com contadores (SysEx, MIDI, notas ativas), seguido das mensagens SysEx mais recentes com identificação do tipo, e os últimos eventos MIDI de canal no final.
