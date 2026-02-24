# MIDI 1.0 vs MIDI 2.0 — Pesquisa Técnica

> Gerado em 2026-02-19 como referência para futura compatibilidade MIDI 2.0 neste projeto.

---

## 1. Mudanças no Protocolo de Mensagens

### MIDI 1.0 — Formato atual

MIDI 1.0 usa um **byte stream assíncrono** a 31.25 kbaud. Mensagens de comprimento variável:

- **Status byte**: 1 byte (MSB=1, nibble alto = tipo, nibble baixo = canal)
- **Data bytes**: 1 ou 2 bytes (MSB=0, valor 0–127, ou seja, **7 bits** de resolução)

```
Note On:        [9n kk vv]   (3 bytes: status, note, velocity)
Note Off:       [8n kk vv]
Control Change: [Bn cc vv]
Pitch Bend:     [En ll hh]   (14 bits, 0–16383)
Prog. Change:   [Cn pp]      (2 bytes)
```

### MIDI 2.0 — Universal MIDI Packet (UMP)

MIDI 2.0 abandona o byte stream e introduz pacotes alinhados a **32 bits**. Cada mensagem ocupa 1, 2, 3 ou 4 palavras de 32 bits.

**Resolução comparada:**

| Parâmetro        | MIDI 1.0              | MIDI 2.0                        |
|------------------|-----------------------|---------------------------------|
| Velocity         | 7 bits (128 valores)  | 16 bits (65.536 valores)        |
| Control Change   | 7 bits (128 valores)  | 32 bits (4.294.967.296 valores) |
| Pitch Bend       | 14 bits (16.384)      | 32 bits                         |
| Per-Note Pitch   | Não existe            | 32 bits por nota                |
| Canais totais    | 16                    | 256 (16 grupos × 16 canais)    |

---

## 2. Universal MIDI Packet (UMP) — Estrutura

### Cabeçalho dos primeiros 4 bits

```
Bits [31:28] = Message Type (MT)
Bits [27:24] = Group (0-15)
Bits [23:xx] = conteúdo específico
```

### Tabela de Message Types

| MT   | Tipo                                    | Tamanho  |
|------|-----------------------------------------|----------|
| 0x0  | Utility (JR Timestamp, etc.)            | 32 bits  |
| 0x1  | System Common & Real Time               | 32 bits  |
| 0x2  | MIDI 1.0 Channel Voice (em UMP)         | 32 bits  |
| 0x3  | SysEx 7-bit segmentado                  | 64 bits  |
| 0x4  | MIDI 2.0 Channel Voice                  | 64 bits  |
| 0x5  | SysEx 8-bit / Mixed Data Set            | 128 bits |
| 0xD  | Flex Data                               | 128 bits |
| 0xF  | UMP Stream (Endpoint Discovery)         | 128 bits |

### Mensagens MIDI 2.0 Channel Voice (MT=0x4) — 64 bits

**Note On/Off:**
```
Word 0: [MT=4][Group 4b][Opcode 4b][Canal 4b][Note 8b][Attribute Type 8b]
Word 1: [Velocity 16b][Attribute Value 16b]
```
- Velocity Note Off 0x0000 = silêncio absoluto (diferente de Note On vel=0 do MIDI 1.0)
- Attribute Type 0x03 = Pitch 7.9 (microafinação com resolução de 1/512 semitom)

**Control Change:**
```
Word 0: [MT=4][Group][0xB][Canal][Index CC 8b][00000000]
Word 1: [Valor CC 32b]
```

**Pitch Bend:**
```
Word 0: [MT=4][Group][0x6][Canal][00000000][00000000]
Word 1: [Valor 32b]   ← 0x80000000 = centro
```

**Per-Note Pitch Bend (novo):**
```
Word 0: [MT=4][Group][0x6 per-note][Canal][Note 8b][00000000]
Word 1: [Valor 32b]
```
Permite pitch bend independente por nota — torna MPE desnecessário na maioria dos casos.

**RPN/NRPN nativos (novo):**
```
Word 0: [MT=4][Group][0x2=RPN ou 0x3=NRPN][Canal][Bank 8b][Index 8b]
Word 1: [Valor 32b]
```
Antes precisava de 4–6 CCs para o mesmo efeito.

---

## 3. MIDI Capability Inquiry (MIDI-CI)

Mecanismo de **negociação de capacidades** via Universal SysEx (F0 7E ... F7) antes de ativar recursos MIDI 2.0.

### Processo de Discovery

```
Iniciador → (SysEx Broadcast Discovery, MUID broadcast 0x0FFFFFFF)
          ← (Reply to Discovery + MUID próprio do dispositivo)
```

### Negociação de Protocolo

```
Iniciador → (Initiate Protocol Negotiation, lista de protocolos aceitos)
          ← (Reply: protocolo confirmado)
Iniciador → (Set New Protocol)
          → (Test New Protocol: mensagem de eco)
          ← (Test New Protocol Response)
Iniciador → (Confirmation New Protocol Established)
```

> **Nota:** Protocol Negotiation via MIDI-CI foi marcado como **deprecated** na UMP v1.1. A abordagem moderna usa UMP Stream Messages (MT=0xF) diretamente no transporte USB MIDI 2.0.

### Três áreas funcionais do MIDI-CI

1. **Protocol Negotiation** — deprecated em favor do UMP Endpoint Discovery
2. **Profile Configuration** — conjuntos predefinidos de mapeamentos por categoria de instrumento (Piano, Drawbar Organ, Percussion, Mixing, Lighting…)
3. **Property Exchange** — troca de metadados JSON via SysEx (lista de patches, nomes de controladores, info do fabricante, etc.)

---

## 4. Retrocompatibilidade MIDI 1.0 ↔ MIDI 2.0

### Regras de conversão (bit-replication)

**MIDI 1.0 → MIDI 2.0 (upscaling):**
```c
// Velocity 7-bit → 16-bit
uint16_t v16 = (v7 << 9) | (v7 << 2) | (v7 >> 5);

// CC 7-bit → 32-bit (mesma técnica)
uint32_t cc32 = (cc7 << 25) | (cc7 << 18) | (cc7 << 11) | (cc7 << 4) | (cc7 >> 3);

// Pitch Bend 14-bit → 32-bit
uint32_t pb32 = (pb14 << 18) | (pb14 << 4) | (pb14 >> 10);
```

**MIDI 2.0 → MIDI 1.0 (downscaling):**
```c
uint8_t  v7   = v16  >> 9;
uint8_t  cc7  = cc32 >> 25;
uint16_t pb14 = pb32 >> 18;
```

---

## 5. USB MIDI 2.0

### Dual Alternate Setting

Um dispositivo USB MIDI 2.0 expõe dois Alternate Settings no mesmo Interface:

- **altset 0:** Descritores USB MIDI 1.0 clássicos (compatível com qualquer host)
- **altset 1:** Descritores USB MIDI 2.0 com Group Terminal Blocks (GTBs), transporta UMP puro

O host seleciona automaticamente o altset mais avançado que suporta.

**Suporte a altset 1:**
- Linux ≥ 6.5
- macOS ≥ 14 (Sonoma)
- Android ≥ 13
- Windows 11 (com Windows MIDI Services)

### Group Terminal Blocks (GTBs)

```c
bGrpTrmBlkID      // ID único (1–255)
bGrpTrmBlkType    // 0x00 = bidirecional
nGroupTrm         // Primeiro grupo UMP (0 = Group 1)
nNumGroupTrm      // Quantos grupos abrange
bMIDIProtocol     // 0x01 = MIDI 1.0, 0x02 = MIDI 2.0
```

### UMP Endpoint Discovery (MT=0xF) sobre USB MIDI 2.0

```
Host → UMP Endpoint Discovery       (MT=0xF, Opcode=0x000)
     ← UMP Endpoint Info Notif.     (MT=0xF, Opcode=0x001)
     ← UMP Endpoint Name Notif.     (MT=0xF, Opcode=0x003)
Host → UMP Function Block Discovery (MT=0xF, Opcode=0x010)
     ← UMP Function Block Info      (MT=0xF, Opcode=0x011)
```

---

## 6. BLE MIDI 2.0

**Ainda não existe especificação oficial de BLE MIDI 2.0** (fevereiro de 2026).

A spec BLE MIDI original (Apple/MMA, 2015) transporta apenas MIDI 1.0. O que é possível hoje:
- Usar o transporte BLE MIDI 1.0 para carregar SysEx de MIDI-CI (Profile Configuration, Property Exchange)
- UMP nativo sobre BLE: aguardando spec oficial da MIDI Association

### Formato do pacote BLE MIDI 1.0 (referência)

```
Byte 0 (Header):    [1][t6..t0]   MSB=1, bits 6-0 = upper timestamp
Byte 1 (Timestamp): [1][t6..t0]   MSB=1, bits 6-0 = lower timestamp
Bytes N+: MIDI status + data bytes normais
Timestamp: 13 bits, unidade = ms, max = 8192 ms
```

---

## 7. O que precisaríamos fazer neste projeto

### `MIDIEventData` — extensão para MIDI 2.0

```cpp
// Campos atuais (MIDI 1.0)
int velocity;     // 0–127 (7 bits)
int pitchBend;    // 0–16383 (14 bits)

// Campos adicionais para MIDI 2.0
uint16_t velocity16;      // 0–65535
uint32_t controlValue32;  // 0–4.294.967.295
uint32_t pitchBend32;     // centro = 0x80000000
uint8_t  attributeType;   // Per-note attribute type
uint16_t attributeValue;  // Per-note attribute value
uint8_t  umpGroup;        // Grupo UMP (0–15)
bool     isMidi2;         // flag MIDI 1.0 vs 2.0
```

### `USBConnection` — suporte a altset 1

- Detectar se o dispositivo conectado expõe altset 1
- Se sim: processar pacotes UMP (32/64 bits) em vez de byte stream
- Se não: continuar com o processamento MIDI 1.0 atual

### `MIDIHandler` — parser UMP

```cpp
// Novo método para processar pacotes UMP
void handleUMPPacket(const uint32_t* words, size_t wordCount);
```

### `BLEConnection` — sem mudanças por enquanto

Aguardar especificação oficial de BLE MIDI 2.0. MIDI-CI pode ser adicionado via SysEx sobre o transporte BLE atual.

### Conversão automática

Implementar as funções de upscaling/downscaling da spec para manter compatibilidade transparente com dispositivos MIDI 1.0.

---

## 8. Bibliotecas de Referência

| Projeto | Descrição |
|---------|-----------|
| [AM_MIDI2.0Lib](https://github.com/midi2-dev/AM_MIDI2.0Lib) | Biblioteca C++ oficial para embedded MIDI 2.0. ~10KB compilado, ~1KB RAM, processa pacote a pacote |
| [tusb_ump](https://github.com/midi2-dev/tusb_ump) | Driver USB MIDI 2.0 sobre TinyUSB (relevante pois o ESP32-S3 usa TinyUSB) |

---

## 9. Fontes

- [MIDI.org — MIDI 2.0 Overview](https://midi.org/midi-2-0)
- [MIDI.org — UMP Specification](https://midi.org/universal-midi-packet-ump-and-midi-2-0-protocol-specification)
- [USB.org — USB MIDI Class Definition v2.0](https://www.usb.org/document-library/usb-class-definition-midi-devices-v20)
- [Linux Kernel — MIDI 2.0 on Linux](https://docs.kernel.org/sound/designs/midi-2.0.html)
- [Microsoft — Windows MIDI Services MIDI 2.0](https://microsoft.github.io/MIDI/kb/midi2-implementation-details/)
- [midi2-dev/AM_MIDI2.0Lib (GitHub)](https://github.com/midi2-dev/AM_MIDI2.0Lib)
- [midi2-dev/tusb_ump (GitHub)](https://github.com/midi2-dev/tusb_ump)
- [Sound On Sound — Introducing MIDI 2.0](https://www.soundonsound.com/music-business/introducing-midi-20)
- [Wikipedia — MIDI 2.0](https://en.wikipedia.org/wiki/MIDI_2.0)
