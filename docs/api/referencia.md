# 📚 Referência da API

Referência completa de todas as classes, estruturas e métodos da biblioteca ESP32_Host_MIDI.

---

## MIDIEventData

Estrutura que representa um evento MIDI parseado. Retornada por `getQueue()`.

```cpp
struct MIDIEventData {
    int index;                // Contador global (único, crescente)
    int msgIndex;             // Liga pares NoteOn ↔ NoteOff
    unsigned long timestamp;  // millis() no momento do evento
    unsigned long delay;      // Δt em ms desde o evento anterior

    int channel;              // Canal MIDI: 1–16
    std::string status;       // Tipo: "NoteOn" | "NoteOff" | "ControlChange" |
                              //       "ProgramChange" | "PitchBend" | "ChannelPressure"
    int note;                 // Número MIDI (0–127) ou número do controlador (CC)
    std::string noteName;     // "C", "C#", "D" ... (vazio para não-notas)
    std::string noteOctave;   // "C4", "D#5", "G3" ... (vazio para não-notas)
    int velocity;             // Velocidade (0–127); também valor de CC, program, pressure
    int chordIndex;           // Índice de agrupamento de acorde (simultâneas = mesmo índice)
    int pitchBend;            // Valor raw 14-bit: 0–16383 (centro = 8192). 0 para outros tipos.
};
```

### Mapeamento por tipo de mensagem

| `status` | `note` | `velocity` | `pitchBend` |
|---------|--------|-----------|------------|
| `"NoteOn"` | Nota MIDI (0–127) | Velocidade (0–127) | 0 |
| `"NoteOff"` | Nota MIDI (0–127) | Release velocity | 0 |
| `"ControlChange"` | CC number (0–127) | CC value (0–127) | 0 |
| `"ProgramChange"` | Program (0–127) | Program (0–127) | 0 |
| `"PitchBend"` | 0 | 0 | 0–16383 (centro=8192) |
| `"ChannelPressure"` | 0 | Pressure (0–127) | 0 |

---

## MIDIHandlerConfig

Struct de configuração passada para `midiHandler.begin(cfg)`.

```cpp
struct MIDIHandlerConfig {
    int maxEvents = 20;
    // Capacidade máxima da fila de eventos. Eventos mais antigos são descartados
    // quando a fila atinge este limite.

    unsigned long chordTimeWindow = 0;
    // Janela de tempo (ms) para agrupar notas simultâneas no mesmo chordIndex.
    // 0 = novo acorde apenas quando TODAS as notas são soltas.
    // 30–80 ms = ideal para teclados físicos.

    int velocityThreshold = 0;
    // Filtra NoteOn com velocity < velocityThreshold. 0 = desabilitado.

    int historyCapacity = 0;
    // Capacidade do buffer histórico circular (em PSRAM se disponível).
    // 0 = desabilitado.

    int maxSysExSize = 512;
    // Tamanho máximo de uma mensagem SysEx (bytes, incluindo F0 e F7).
    // Mensagens maiores são truncadas. 0 = desabilita SysEx.

    int maxSysExEvents = 8;
    // Capacidade da fila de SysEx. Mensagens mais antigas são descartadas.

    const char* bleName = "ESP32 MIDI BLE";
    // Nome anunciado pelo periférico BLE MIDI.
};
```

---

## MIDIHandler

Singleton global: `extern MIDIHandler midiHandler;`

### Setup

```cpp
void begin();
// Inicializa com configuração padrão.
// Registra automaticamente: USBConnection (se S2/S3/P4), BLEConnection (se BT habilitado)

void begin(const MIDIHandlerConfig& config);
// Inicializa com configuração personalizada.

void addTransport(MIDITransport* transport);
// Registra um transporte externo (até 4 transportes externos adicionais).
// Deve ser chamado ANTES de begin().

void setQueueLimit(int maxEvents);
// Altera a capacidade da fila após begin().

void enableHistory(int capacity);
// Ativa buffer histórico circular (usa PSRAM se disponível, fallback para heap).
// Pode ser chamado após begin().
```

### Loop

```cpp
void task();
// Chame em todo loop(). Drena os ring buffers de todos os transportes,
// parseia mensagens, atualiza fila, notas ativas e acordes.
```

### Recepção — Fila de Eventos

```cpp
const std::deque<MIDIEventData>& getQueue() const;
// Retorna a fila de eventos desde a última chamada de task().
// Itere com range-for: for (const auto& ev : midiHandler.getQueue()) {}

void clearQueue();
// Esvazia a fila imediatamente.
```

### Recepção — Notas Ativas

```cpp
std::string getActiveNotes() const;
// Retorna string formatada: "{C4, E4, G4}"

std::string getActiveNotesString() const;
// Alias de getActiveNotes()

std::vector<std::string> getActiveNotesVector() const;
// Retorna vetor de strings: ["C4", "E4", "G4"]

size_t getActiveNotesCount() const;
// Número de notas atualmente pressionadas.

void fillActiveNotes(bool out[128]) const;
// Preenche array[128] — out[note] = true se a nota estiver ativa.

void clearActiveNotesNow();
// Zera o mapa de notas ativas (útil após reconexão ou reset).
```

### Recepção — Acordes

```cpp
int lastChord(const std::deque<MIDIEventData>& queue) const;
// Retorna o chordIndex mais recente na fila. -1 se vazia.

std::vector<std::string> getChord(
    int chord,
    const std::deque<MIDIEventData>& queue,
    const std::vector<std::string>& fields = {"all"},
    bool includeLabels = false
) const;
// Retorna valores de campo(s) para todas as notas com chordIndex == chord.
// fields: {"noteOctave"} | {"noteName"} | {"velocity"} | {"all"} | combinações
// includeLabels: prefixar cada valor com "campo:valor"

std::vector<std::string> getAnswer(
    const std::string& field = "all",
    bool includeLabels = false
) const;
// Atalho para lastChord + getChord com um único campo.

std::vector<std::string> getAnswer(
    const std::vector<std::string>& fields,
    bool includeLabels = false
) const;
// Atalho para lastChord + getChord com múltiplos campos.
```

### Recepção — SysEx

```cpp
const std::deque<MIDISysExEvent>& getSysExQueue() const;
// Retorna a fila de SysEx. Separada da fila de eventos normais.
// Cada MIDISysExEvent contém: index, timestamp, data (vector<uint8_t>).

void clearSysExQueue();
// Esvazia a fila de SysEx.

bool sendSysEx(const uint8_t* data, size_t length);
// Envia SysEx para todos os transportes. Deve começar com 0xF0 e terminar com 0xF7.
// Retorna false se a mensagem for inválida.

typedef void (*SysExCallback)(const uint8_t* data, size_t length);
void setSysExCallback(SysExCallback cb);
// Callback opcional chamado imediatamente ao receber uma mensagem SysEx completa.
// Use nullptr para desabilitar.
```

### MIDISysExEvent

```cpp
struct MIDISysExEvent {
    int index;                      // Contador global crescente
    unsigned long timestamp;        // millis() no momento da recepção
    std::vector<uint8_t> data;      // Mensagem completa (F0 ... F7)
};
```

---

### Envio de MIDI

Todos os métodos de envio transmitem para **todos os transportes** que suportam envio:

```cpp
bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
// channel: 1–16 | note: 0–127 | velocity: 0–127
// Retorna true se pelo menos um transporte enviou.

bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
// velocity tipicamente 0 para NoteOff.

bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value);
// controller: 0–127 (CC#7=volume, CC#11=expression, CC#64=sustain, etc.)
// value: 0–127

bool sendProgramChange(uint8_t channel, uint8_t program);
// program: 0–127

bool sendPitchBend(uint8_t channel, int value);
// value: -8192 a +8191 (convertido internamente para 0–16383)

bool sendRaw(const uint8_t* data, size_t length);
// Envia bytes MIDI crus para todos os transportes.

bool sendBleRaw(const uint8_t* data, size_t length);
// Alias de sendRaw() (compatibilidade retroativa).
```

### BLE Status

```cpp
#if ESP32_HOST_MIDI_HAS_BLE
bool isBleConnected() const;
// Retorna true se um dispositivo BLE MIDI estiver conectado.
#endif
```

### Debug Callback

```cpp
typedef void (*RawMidiCallback)(const uint8_t* raw, size_t rawLen,
                                 const uint8_t* midi3);
void setRawMidiCallback(RawMidiCallback cb);
// cb é chamado com os bytes MIDI crus ANTES do parsing.
// raw = payload USB-MIDI completo; midi3 = os 3 bytes MIDI.
// Use nullptr para desabilitar.
```

---

## MIDITransport

Interface abstrata implementada por todos os transportes.

```cpp
class MIDITransport {
public:
    virtual ~MIDITransport() = default;

    // Obrigatório — implementar nas subclasses:
    virtual void task() = 0;                // Chamado a cada loop() pelo MIDIHandler
    virtual bool isConnected() const = 0;   // Status atual da conexão

    // Opcional — implementar nas subclasses:
    virtual bool sendMidiMessage(const uint8_t* data, size_t length);
    // Retorna true se enviou. Default: return false.

    // Registro de callbacks (chamado pelo MIDIHandler):
    void setMidiCallback(MidiDataCallback cb, void* ctx);
    void setSysExCallback(SysExDataCallback cb, void* ctx);
    void setConnectionCallbacks(ConnectionCallback onConnected,
                                ConnectionCallback onDisconnected,
                                void* ctx);

protected:
    // Chamar nas subclasses para injetar dados no MIDIHandler:
    void dispatchMidiData(const uint8_t* data, size_t len);
    void dispatchSysExData(const uint8_t* data, size_t len);
    void dispatchConnected();
    void dispatchDisconnected();
};
```

---

## USBConnection

Transporte USB Host. Incluído automaticamente em chips S2/S3/P4.

```cpp
// Uso interno — não instancie diretamente.
// Configuração: Tools > USB Mode → "USB Host"
```

---

## BLEConnection

Transporte BLE MIDI. Incluído automaticamente se `CONFIG_BT_ENABLED`.

```cpp
// Uso interno — não instancie diretamente.
// Nome configurado via MIDIHandlerConfig::bleName
```

---

## ESPNowConnection

```cpp
#include "src/ESPNowConnection.h"

ESPNowConnection espNow;
espNow.begin(int channel = 0);     // 0 = usar canal atual do WiFi
espNow.addPeer(const uint8_t mac[6]);  // Adicionar peer para unicast
// isConnected() sempre retorna true
```

---

## UARTConnection

```cpp
#include "src/UARTConnection.h"

UARTConnection uart;
uart.begin(HardwareSerial& serial, int rxPin, int txPin);
// serial: Serial1, Serial2, etc.
// rxPin, txPin: GPIOs para RX e TX
// isConnected() sempre retorna true
```

---

## RTPMIDIConnection

```cpp
#include "src/RTPMIDIConnection.h"  // Requer AppleMIDI-Library v3.x

RTPMIDIConnection rtpMIDI;
rtpMIDI.begin(const char* sessionName = "ESP32 MIDI");
// WiFi deve estar conectado ANTES de chamar begin()
// isConnected() retorna true quando há sessão AppleMIDI ativa
```

---

## EthernetMIDIConnection

```cpp
#include "src/EthernetMIDIConnection.h"  // Requer AppleMIDI + Ethernet

EthernetMIDIConnection ethMIDI;
ethMIDI.begin(const uint8_t mac[6]);                          // DHCP
ethMIDI.begin(const uint8_t mac[6], IPAddress staticIP);      // IP estático
ethMIDI.begin(const uint8_t mac[6], IPAddress ip, int csPin); // CS pin customizado
```

---

## OSCConnection

```cpp
#include "src/OSCConnection.h"  // Requer CNMAT/OSC

OSCConnection osc;
osc.begin(
    int localPort,              // Porta UDP local (ex: 8000)
    IPAddress remoteIP,         // IP do destino (ex: computador com Max/MSP)
    int remotePort              // Porta UDP do destino (ex: 9000)
);
```

Mapa de endereços:
```
/midi/noteon    channel note velocity
/midi/noteoff   channel note velocity
/midi/cc        channel controller value
/midi/pc        channel program
/midi/pitchbend channel bend
/midi/aftertouch channel pressure
```

---

## USBDeviceConnection

```cpp
#include "src/USBDeviceConnection.h"
// Configuração: Tools > USB Mode → "USB-OTG (TinyUSB)"

USBDeviceConnection usbDev(const char* deviceName = "ESP32 MIDI");
usbDev.begin();
// Deve ser chamado ANTES de midiHandler.begin()
// isConnected() retorna true quando o host USB estiver conectado
```

---

## MIDI2Support — Tipos e Utilitários

```cpp
#include "src/MIDI2Support.h"

// Escalamento
uint16_t MIDI2Scaler::scale7to16(uint8_t v7);
uint32_t MIDI2Scaler::scale7to32(uint8_t v7);
uint32_t MIDI2Scaler::scale14to32(uint16_t v14);
uint8_t  MIDI2Scaler::scale16to7(uint16_t v16);
uint8_t  MIDI2Scaler::scale32to7(uint32_t v32);
uint16_t MIDI2Scaler::scale32to14(uint32_t v32);

// Builder UMP
UMPWord64 UMPBuilder::noteOn(uint8_t group, uint8_t channel,
                              uint8_t note, uint16_t velocity16);
UMPWord64 UMPBuilder::noteOff(uint8_t group, uint8_t channel,
                               uint8_t note, uint16_t velocity16);
UMPWord64 UMPBuilder::controlChange(uint8_t group, uint8_t channel,
                                     uint8_t index, uint32_t value32);
UMPWord64 UMPBuilder::pitchBend(uint8_t group, uint8_t channel,
                                 uint32_t value32);

// Parser UMP
UMPResult UMPParser::parseMIDI2(UMPWord64 pkt);
```

---

## GingoAdapter

```cpp
#include "src/GingoAdapter.h"  // Requer Gingoduino ≥ v0.2.2

// Identificar nome do acorde mais recente
bool GingoAdapter::identifyLastChord(
    MIDIHandler& handler,
    char* outName,
    size_t nameSize
);

// Converter notas MIDI para GingoNote
uint8_t GingoAdapter::midiToGingoNotes(
    const uint8_t* midiNotes,
    uint8_t count,
    GingoNote* outNotes
);

// Campo harmônico (requer GINGODUINO_HAS_FIELD)
#if defined(GINGODUINO_HAS_FIELD)
uint8_t GingoAdapter::deduceFieldFromQueue(
    MIDIHandler& handler,
    FieldMatch* outFields,
    uint8_t maxFields
);
#endif

// Progressão (requer GINGODUINO_HAS_PROGRESSION)
#if defined(GINGODUINO_HAS_PROGRESSION)
bool GingoAdapter::identifyProgression(
    const char* root,
    ScaleType scale,
    const char** branches,
    uint8_t branchCount,
    ProgressionMatch* out
);
#endif
```

---

## Macros de Feature Detection

```cpp
ESP32_HOST_MIDI_HAS_USB    // 1 se chip suportar USB OTG (S2, S3, P4)
ESP32_HOST_MIDI_HAS_BLE    // 1 se CONFIG_BT_ENABLED
ESP32_HOST_MIDI_HAS_PSRAM  // 1 se CONFIG_SPIRAM ou CONFIG_SPIRAM_SUPPORT
ESP32_HOST_MIDI_HAS_ETH_MAC // 1 se ESP32-P4 (MAC Ethernet nativo)
```

---

## Notas de Uso

- `midiHandler.task()` deve ser chamado em **todo** `loop()`, sem bloqueios longos
- `addTransport()` deve ser chamado **antes** de `begin()`
- O máximo de transportes externos é **4** (built-ins não contam)
- Ring buffers são thread-safe com `portMUX` — seguro para FreeRTOS
- A fila (`getQueue()`) é válida apenas dentro da iteração — não guarde referências além do loop atual
