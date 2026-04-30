# ⚙️ Configuração

O `MIDIHandler` pode ser configurado via `MIDIHandlerConfig` antes de chamar `begin()`.

---

## MIDIHandlerConfig

```cpp
struct MIDIHandlerConfig {
    int maxEvents        = 20;    // Capacidade da fila de eventos
    unsigned long chordTimeWindow = 0;  // Janela para detecção de acordes (ms)
    int velocityThreshold = 0;   // Filtro mínimo de velocidade (0 = desabilitado)
    int historyCapacity  = 0;    // Buffer histórico PSRAM (0 = desabilitado)
    int maxSysExSize     = 512;  // Tamanho máximo de uma SysEx
    int maxSysExEvents   = 8;    // Profundidade da fila de SysEx
    const char* bleName  = "ESP32 MIDI BLE";  // legacy v5 (ver nota)
};
```

!!! note "v6.0+: bleName não é mais consumido pelo MIDIHandler"
    O campo `bleName` continua na struct para compatibilidade de código antigo, mas em v6+ o `MIDIHandler::begin(cfg)` não inicializa nenhum transporte BLE automaticamente. Passe o nome direto pra `BLEConnection::begin(name)` quando registrar o transporte.

### Uso básico

```cpp
#include <BLEConnection.h>

BLEConnection bleHost;     // v6.0+: cada transporte é explícito

void setup() {
    MIDIHandlerConfig cfg;
    cfg.maxEvents = 30;
    midiHandler.addTransport(&bleHost);
    bleHost.begin("Meu Sintetizador");   // nome BLE vai aqui agora
    midiHandler.begin(cfg);
}
```

---

## maxEvents — Capacidade da Fila

`maxEvents` define quantos eventos MIDI a fila mantém. Quando a fila atinge o limite, eventos mais antigos são descartados (FIFO).

```cpp
// Padrão: 20 eventos
cfg.maxEvents = 20;

// Para uso educacional (ver histórico longo):
cfg.maxEvents = 50;

// Para uso em loop() frequente (mais reativo):
cfg.maxEvents = 10;
```

!!! note "O que é a fila?"
    A fila (`getQueue()`) contém os eventos **desde a última chamada de `task()`**. Se o seu `loop()` for rápido, raramente terá mais de 5–10 eventos ao mesmo tempo.

Você também pode alterar após `begin()`:

```cpp
midiHandler.setQueueLimit(50);
```

---

## chordTimeWindow — Detecção de Acordes

Controla como notas simultâneas são agrupadas no mesmo `chordIndex`.

```cpp
// 0 ms (padrão): novo acorde apenas quando TODAS as notas são soltas
cfg.chordTimeWindow = 0;

// 30–80 ms: mais reativo para teclados físicos
// (pequenos atrasos entre os dedos ao tocar acordes)
cfg.chordTimeWindow = 50;
```

```mermaid
sequenceDiagram
    participant KEY as Teclado
    participant HANDLER as MIDIHandler

    Note over KEY,HANDLER: chordTimeWindow = 50ms

    KEY->>HANDLER: NoteOn C4 (t=0ms)
    Note right of HANDLER: chordIndex = 1
    KEY->>HANDLER: NoteOn E4 (t=20ms)
    Note right of HANDLER: chordIndex = 1 (dentro da janela)
    KEY->>HANDLER: NoteOn G4 (t=45ms)
    Note right of HANDLER: chordIndex = 1 (dentro da janela)
    KEY->>HANDLER: NoteOn B4 (t=120ms)
    Note right of HANDLER: chordIndex = 2 (nova janela!)
```

!!! tip "Quando usar chordTimeWindow?"
    - **0** (padrão): mais preciso, ideal quando você controla o timing (sequenciadores)
    - **30–80 ms**: ideal para teclados físicos onde os dedos chegam em tempos ligeiramente diferentes

Veja mais em [Detecção de Acordes →](../funcionalidades/deteccao-acordes.md)

---

## velocityThreshold — Filtro de Velocidade

Ignora eventos NoteOn com velocidade abaixo do limiar. Útil para filtrar "ghost notes" de sensores piezo.

```cpp
// Desabilitado (padrão) — processa todas as velocidades
cfg.velocityThreshold = 0;

// Filtrar notas muito suaves (veloc < 10)
cfg.velocityThreshold = 10;

// Apenas notas fortes (percussão):
cfg.velocityThreshold = 40;
```

---

## historyCapacity — Histórico em PSRAM

Habilita um buffer circular de eventos que persiste além do limite de `maxEvents`. Usa PSRAM quando disponível, com fallback para heap.

```cpp
// Desabilitado (padrão)
cfg.historyCapacity = 0;

// Guardar os últimos 500 eventos
cfg.historyCapacity = 500;
```

Também pode ser ativado após `begin()`:

```cpp
midiHandler.enableHistory(500);
```

!!! warning "PSRAM necessário para históricos grandes"
    Históricos grandes (> 200 eventos) devem usar ESP32-S3 com PSRAM (4 MB+). Sem PSRAM, o buffer é alocado no heap — espaço limitado.

Veja mais em [Histórico PSRAM →](../funcionalidades/historico-psram.md)

---

## bleName — Nome do Dispositivo BLE (legacy)

Em v5, `cfg.bleName` era passado pelo `MIDIHandler::begin()` para o transporte BLE auto-instanciado. Em v6+ o `MIDIHandler` não auto-instancia transportes; o nome BLE vai direto pra `BLEConnection::begin(name)`:

```cpp
// v6+: passe o nome ao BLEConnection diretamente
BLEConnection bleHost;
bleHost.begin("Piano ESP32");        // aparece em GarageBand, AUM, etc.
midiHandler.addTransport(&bleHost);
midiHandler.begin();
```

O campo `cfg.bleName` continua na struct por compatibilidade binária, mas é ignorado pelo handler em v6+.

---

## Macros de Feature Detection

A biblioteca detecta automaticamente os recursos disponíveis com base no chip alvo:

```cpp
// Compilado automaticamente — não defina manualmente

// USB Host disponível (ESP32-S2, S3, P4)
#if ESP32_HOST_MIDI_HAS_USB
    // Teclado USB conectado!
#endif

// BLE disponível (ESP32, S3, C3, C6 — CONFIG_BT_ENABLED)
#if ESP32_HOST_MIDI_HAS_BLE
    // v6.0+: consulte a instância de BLEConnection diretamente.
    // MIDIHandler::isBleConnected() foi removido junto com o member built-in.
    bool connected = bleHost.isConnected();
#endif

// PSRAM disponível (CONFIG_SPIRAM ou CONFIG_SPIRAM_SUPPORT)
#if ESP32_HOST_MIDI_HAS_PSRAM
    midiHandler.enableHistory(1000);  // 1000 eventos em PSRAM
#endif

// Ethernet MAC nativo (apenas ESP32-P4)
#if ESP32_HOST_MIDI_HAS_ETH_MAC
    // Use EthernetMIDIConnection com PHY externo (LAN8720)
#endif
```

---

## Debug Callback — Raw MIDI

Para inspecionar os bytes MIDI brutos antes do parsing:

```cpp
void onRawMidi(const uint8_t* raw, size_t rawLen, const uint8_t* midi3) {
    // raw     = payload USB-MIDI completo (CIN + 3 bytes)
    // rawLen  = tamanho do raw
    // midi3   = os 3 bytes MIDI (status, data1, data2)
    Serial.printf("Raw: %02X %02X %02X\n",
        midi3[0], midi3[1], midi3[2]);
}

void setup() {
    midiHandler.addTransport(&usbHost);   // v6.0+: registre seus transportes
    usbHost.begin();
    midiHandler.setRawMidiCallback(onRawMidi);
    midiHandler.begin();
}
```

---

## Limpar a Fila e Notas Ativas

```cpp
// Esvazia a fila de eventos imediatamente
midiHandler.clearQueue();

// Zera o mapa de notas ativas (útil ao reconectar)
midiHandler.clearActiveNotesNow();
```

---

## Exemplo — Configuração Completa

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>      // v6.0+: cada transporte explícito
#include <BLEConnection.h>

USBConnection usbHost;
BLEConnection bleHost;

void setup() {
    Serial.begin(115200);

    MIDIHandlerConfig cfg;
    cfg.maxEvents         = 30;     // fila maior
    cfg.chordTimeWindow   = 50;     // agrupar notas de acordes
    cfg.velocityThreshold = 5;      // ignorar ghost notes
    cfg.historyCapacity   = 500;    // guardar histórico em PSRAM

    midiHandler.addTransport(&usbHost);
    midiHandler.addTransport(&bleHost);
    usbHost.begin();
    bleHost.begin("Meu ESP32");     // nome BLE direto no transport
    midiHandler.begin(cfg);
}
```

---

## Próximos Passos

- [Transportes →](../transportes/visao-geral.md) — adicionar mais transportes
- [Detecção de Acordes →](../funcionalidades/deteccao-acordes.md) — usar `chordTimeWindow`
- [Histórico PSRAM →](../funcionalidades/historico-psram.md) — usar `historyCapacity`
