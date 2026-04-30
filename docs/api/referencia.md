# 📚 Referência da API

Referência completa de todas as classes, estruturas e métodos da biblioteca ESP32_Host_MIDI.

---

## MIDIStatus

Enum com os status bytes reais do protocolo MIDI:

```cpp
enum MIDIStatus : uint8_t {
    MIDI_NOTE_OFF          = 0x80,
    MIDI_NOTE_ON           = 0x90,
    MIDI_POLY_PRESSURE     = 0xA0,
    MIDI_CONTROL_CHANGE    = 0xB0,
    MIDI_PROGRAM_CHANGE    = 0xC0,
    MIDI_CHANNEL_PRESSURE  = 0xD0,
    MIDI_PITCH_BEND        = 0xE0,
};
```

---

## MIDIEventData

Estrutura que representa um evento MIDI parseado. Retornada por `getQueue()`.

```cpp
struct MIDIEventData {
    int index;                // Contador global (único, crescente)
    int msgIndex;             // Liga pares NoteOn ↔ NoteOff
    unsigned long timestamp;  // millis() no momento do evento
    unsigned long delay;      // Δt em ms desde o evento anterior

    // v5.2+ — MIDI spec compliant
    MIDIStatus statusCode;    // MIDI_NOTE_ON | MIDI_NOTE_OFF | MIDI_CONTROL_CHANGE | ...
    uint8_t channel0;         // Canal MIDI: 0–15 (spec MIDI)
    uint8_t noteNumber;       // Número MIDI (0–127)
    uint16_t velocity16;      // Velocidade 16-bit (MIDI 2.0, escalado via MIDI2Scaler)
    uint8_t velocity7;        // Velocidade 7-bit (original MIDI 1.0)
    uint32_t pitchBend32;     // Pitch bend 32-bit (MIDI 2.0, centro = 0x80000000)
    uint16_t pitchBend14;     // Pitch bend 14-bit (original, 0–16383, centro = 8192)
    int chordIndex;           // Índice de agrupamento de acorde

    // Deprecated (v5.2) — serão removidos na v6.0
    int channel;              // 1–16 (use channel0)
    std::string status;       // "NoteOn" etc (use statusCode)
    int note;                 // 0–127 (use noteNumber)
    std::string noteName;     // (use MIDIHandler::noteName())
    std::string noteOctave;   // (use MIDIHandler::noteWithOctave())
    int velocity;             // 0–127 (use velocity7 ou velocity16)
    int pitchBend;            // 0–16383 (use pitchBend14 ou pitchBend32)
};
```

### Mapeamento por tipo de mensagem

| `statusCode` | `noteNumber` | `velocity7` | `pitchBend14` |
|---------|--------|-----------|------------|
| `MIDI_NOTE_ON` | Nota MIDI (0–127) | Velocidade (0–127) | 0 |
| `MIDI_NOTE_OFF` | Nota MIDI (0–127) | Release velocity | 0 |
| `MIDI_CONTROL_CHANGE` | CC number (0–127) | CC value (0–127) | 0 |
| `MIDI_PROGRAM_CHANGE` | Program (0–127) | 0 | 0 |
| `MIDI_PITCH_BEND` | 0 | 0 | 0–16383 (centro=8192) |
| `MIDI_CHANNEL_PRESSURE` | 0 | Pressure (0–127) | 0 |

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
    // legacy v5: o handler passava esse nome ao BLEConnection auto-instanciado.
    // Em v6+ o handler não auto-instancia transportes; passe o nome direto pra
    // BLEConnection::begin(name). Campo mantido por compatibilidade binária.
};
```

---

## MIDIHandler

Singleton global: `extern MIDIHandler midiHandler;`

### Setup

```cpp
void begin();
// Inicializa com configuração padrão. Em v6+ NÃO registra nenhum transporte:
// o user instancia cada transport (USBConnection, BLEConnection, etc.) e
// chama addTransport() antes de begin().

void begin(const MIDIHandlerConfig& config);
// Inicializa com configuração personalizada. Mesmo contrato que begin():
// transports devem ser registrados via addTransport() antes desta chamada.

void addTransport(MIDITransport* transport);
// Registra um transporte. Suporta até 4 transports.
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

### Helpers Estáticos

```cpp
static const char* MIDIHandler::noteName(uint8_t noteNumber);
// Retorna nome da nota: "C", "C#", "D", ... (string literal, zero allocation)

static int MIDIHandler::noteOctave(uint8_t noteNumber);
// Retorna oitava: -1 a 9

static const char* MIDIHandler::noteWithOctave(uint8_t noteNumber, char* buf, size_t bufLen);
// Escreve "C4", "D#5" etc em buf (buffer do caller). Retorna buf.

static const char* MIDIHandler::statusName(MIDIStatus code);
// Retorna nome do status: "NoteOn", "NoteOff", "ControlChange", etc.
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

> **Nota:** A API de envio usa canal 1–16 em v5.x e v6.x. A possível migração para 0–15 fica para uma versão futura.

Todos os métodos de envio fazem **fan-out**: tentam cada transporte registrado em ordem e retornam `true` no primeiro que aceitar. Não é broadcast simultâneo.

```cpp
bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
// channel: 1–16 | note: 0–127 | velocity: 0–127
// Retorna true se algum transporte aceitou.

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
// Envia bytes MIDI crus, mesmo padrão fan-out.

bool sendBleRaw(const uint8_t* data, size_t length);
// Alias de sendRaw() (compatibilidade retroativa).
```

### BLE Status (v6+: consultar a instância de BLEConnection)

```cpp
// v6+: o método MIDIHandler::isBleConnected foi removido junto com o
// member built-in BLEConnection. Consulte direto na sua instância:
BLEConnection bleHost;
// ...
if (bleHost.isConnected()) { /* BLE central conectado */ }
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

    // UMP (MIDI 2.0 nativo)
    void setUMPCallback(UMPDataCallback cb, void* ctx);

protected:
    // Chamar nas subclasses para injetar dados no MIDIHandler:
    void dispatchMidiData(const uint8_t* data, size_t len);
    void dispatchSysExData(const uint8_t* data, size_t len);
    void dispatchUMPData(const uint32_t* words, uint8_t count);  // inject UMP words
    void dispatchConnected();
    void dispatchDisconnected();
};
```

---

## USBConnection

Transporte USB Host MIDI 1.0. Em v6+ o user instancia explicitamente.

```cpp
#include <USBConnection.h>

USBConnection usbHost;       // global; TinyUSB precisa antes de begin
midiHandler.addTransport(&usbHost);
usbHost.begin();             // inicia stack USB Host (FreeRTOS task no core 0)
// Configuração Arduino IDE: Tools > USB Mode → "USB Host"
```

---

## USBMIDI2Connection

USB Host com suporte nativo a MIDI 2.0/UMP. Estende `USBConnection`.

```cpp
#include "src/USBMIDI2Connection.h"

USBMIDI2Connection usb;

// Callbacks
usb.setUMPCallback(onUMP, nullptr);   // MIDI 2.0 nativo (UMP words)
usb.setMidiCallback(onMidi, nullptr); // fallback MIDI 1.0

// Consultar capacidades após negociação
usb.isMIDI2();        // true se dispositivo negociou MIDI 2.0
usb.isNegotiated();   // true se Protocol Negotiation completou
usb.getEndpointInfo(); // UMP version, function blocks, protocol
usb.getFunctionBlocks(); // Function Block Info array
usb.getGTBlocks();       // Group Terminal Block array
usb.getEndpointName();   // nome do endpoint (string do dispositivo)
usb.sendUMPMessage(words, count); // enviar UMP words via OUT endpoint
```

---

## BLEConnection

Transporte BLE MIDI peripheral. Em v6+ o user instancia explicitamente.

```cpp
#include <BLEConnection.h>

BLEConnection bleHost;
midiHandler.addTransport(&bleHost);
bleHost.begin("Meu ESP32");  // nome anunciado pelo BLE peripheral

// Status:
if (bleHost.isConnected()) { /* central conectada */ }
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
- O máximo de transports é **4** (em v6+ não há built-ins; todos contam)
- Cada transport precisa do seu próprio `begin()`. O `MIDIHandler::begin()` não chama `begin()` em nenhum transport
- Ring buffers são thread-safe com `portMUX` — seguro para FreeRTOS
- A fila (`getQueue()`) é válida apenas dentro da iteração — não guarde referências além do loop atual
