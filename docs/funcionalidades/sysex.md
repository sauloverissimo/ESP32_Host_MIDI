# 🔧 SysEx (System Exclusive)

O `MIDIHandler` suporta envio e recepção de mensagens SysEx em todos os transportes — USB Host, USB Device e UART (DIN-5). A implementação usa uma fila separada da fila de eventos normais, sem breaking changes para código existente.

---

## O que é SysEx

SysEx (System Exclusive) são mensagens MIDI de tamanho variável delimitadas por `0xF0` (início) e `0xF7` (fim). Diferente de NoteOn, CC ou PitchBend que têm tamanho fixo (2-3 bytes), uma mensagem SysEx pode ter de poucos bytes a vários kilobytes.

Enquanto mensagens de canal (notas, CCs) são padronizadas e genéricas, SysEx carrega dados **proprietários ou universais** que vão muito além do que o MIDI convencional oferece.

---

## O que o SysEx abre de possibilidades

**Identificação de dispositivos** — envie um Universal Identity Request (`F0 7E 7F 06 01 F7`) e receba de volta o manufacturer ID, família, modelo e versão de firmware. Seu ESP32 pode auto-detectar o que está conectado e adaptar o comportamento. Plugou um Roland, um Korg, um Novation — o ESP32 sabe com quem está falando.

**Gerenciamento de patches** — a maioria dos sintetizadores usa SysEx para bulk dumps: patches, presets, wavetables, configurações inteiras. Com o buffer configurável você pode receber um dump completo, salvar em SPIFFS/SD e enviar de volta depois. Seu ESP32 vira um patch librarian.

**Controle profundo de parâmetros** — muitos dispositivos expõem parâmetros via SysEx que não existem como CC. Filter types, oscillator shapes, effects routing — coisas que vão muito além dos 128 valores de CC. Alguns dispositivos têm centenas de parâmetros acessíveis só via SysEx.

**Atualização de firmware** — alguns dispositivos MIDI aceitam update de firmware via SysEx. Com `sendSysEx()` dá pra construir um atualizador baseado em ESP32.

**MIDI Sample Dump** — existe um padrão (MMA Sample Dump Standard) para transferir samples de áudio via SysEx. É lento, mas funciona, e alguns samplers vintage só suportam esse método.

---

## Como funciona internamente

### Remontagem no nível do transporte

Cada transporte lida com a remontagem de forma diferente:

**USB (Host e Device)** — O protocolo USB MIDI 1.0 fragmenta SysEx em pacotes de 4 bytes com um Code Index Number (CIN) no header:

| CIN | Significado | Bytes de dados |
|-----|-------------|----------------|
| `0x04` | SysEx começa ou continua | 3 |
| `0x05` | SysEx termina com 1 byte | 1 |
| `0x06` | SysEx termina com 2 bytes | 2 |
| `0x07` | SysEx termina com 3 bytes | 3 |

A biblioteca acumula os pacotes CIN 0x04 em um buffer interno e, ao receber 0x05/06/07, monta a mensagem completa e despacha via `dispatchSysExData()`.

**UART (DIN-5)** — MIDI serial é mais simples: os bytes chegam sequencialmente. O parser acumula tudo entre `0xF0` e `0xF7`. Se um status byte diferente chegar no meio (erro de protocolo), o SysEx é abortado e o novo status é processado normalmente.

### Fila separada

SysEx usa sua própria `std::deque<MIDISysExEvent>`, completamente separada da fila de eventos (`getQueue()`). Isso foi uma decisão de design proposital — mensagens SysEx são de tamanho variável e misturá-las na fila existente quebraria a API para quem já usa `getQueue()`.

---

## Configuração

```cpp
MIDIHandlerConfig config;
config.maxSysExSize   = 512;  // máximo de bytes por mensagem (inclui F0 e F7)
config.maxSysExEvents = 8;    // quantas mensagens manter na fila
midiHandler.begin(config);
```

| Parâmetro | Default | Descrição |
|-----------|---------|-----------|
| `maxSysExSize` | 512 | Tamanho máximo de uma mensagem. Mensagens maiores são truncadas. 0 = desabilita SysEx. |
| `maxSysExEvents` | 8 | Capacidade da fila. Mensagens mais antigas são descartadas. |

!!! tip "Sobre o tamanho do buffer"
    O ESP32-S3 tem 512KB de SRAM. Mesmo `maxSysExSize = 2048` funciona tranquilo. O buffer é alocado no heap (`std::vector<uint8_t>`), então considere a RAM disponível se estiver rodando BLE, WiFi e display simultaneamente.

---

## API

### Recepção

```cpp
// Acessar a fila de SysEx
const auto& queue = midiHandler.getSysExQueue();

for (const auto& msg : queue) {
    // msg.index     — contador global (crescente)
    // msg.timestamp — millis() no momento da recepção
    // msg.data      — std::vector<uint8_t>, mensagem completa (F0 ... F7)

    Serial.printf("SysEx #%d (%d bytes): ", msg.index, msg.data.size());
    for (uint8_t b : msg.data) Serial.printf("%02X ", b);
    Serial.println();
}

// Limpar a fila
midiHandler.clearSysExQueue();
```

### Callback em tempo real

```cpp
// Opcional: callback imediato em vez de polling
midiHandler.setSysExCallback([](const uint8_t* data, size_t len) {
    // Chamado assim que a mensagem completa é montada
    // data[0] = 0xF0, data[len-1] = 0xF7
});
```

### Envio

```cpp
// Enviar SysEx — transmite para todos os transportes
const uint8_t identityReq[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
bool ok = midiHandler.sendSysEx(identityReq, sizeof(identityReq));
// Retorna false se a mensagem não começar com F0 ou não terminar com F7
```

---

## Compatibilidade

A implementação segue a spec USB MIDI 1.0 (remontagem por CIN) e o framing padrão de MIDI serial. Funciona com qualquer dispositivo class-compliant — se ele manda SysEx válido entre `F0` e `F7`, a biblioteca captura.

!!! warning "Error checking"
    SysEx não tem error checking nativo na spec MIDI. O transporte USB é confiável (CRC no nível USB), mas UART/DIN-5 é serial cru. Alguns dispositivos implementam checksums próprios dentro do payload SysEx — a biblioteca entrega os bytes crus pra que você valide no nível da aplicação.

---

## Exemplo completo

Veja o exemplo [`T-Display-S3-SysEx`](https://github.com/sauloverissimo/ESP32_Host_MIDI/tree/main/examples/T-Display-S3-SysEx) — monitor SysEx com Identity Request no botão, display em tempo real:

![T-Display-S3 SysEx Monitor](https://github.com/sauloverissimo/ESP32_Host_MIDI/raw/main/examples/T-Display-S3-SysEx/images/sysex.jpeg)
