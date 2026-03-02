// AM-MIDI2-Adapter — integração com AM_MIDI2.0Lib
//
// Demonstra como usar a biblioteca padrão mundial para MIDI 2.0
// (AM_MIDI2.0Lib, endossada pela MIDI Association) em conjunto com
// ESP32_Host_MIDI via MIDI2Adapter.
//
// O que este exemplo faz:
//   - Recebe MIDI 1.0 por USB ou BLE
//   - Converte automaticamente para UMP (Universal MIDI Packet)
//   - Entrega ao umpProcessor com valores de alta resolução MIDI 2.0
//   - Mostra dados MIDI 1.0 (compatibilidade) e MIDI 2.0 (alta res) no Serial
//
// Pré-requisito — instale AM_MIDI2.0Lib:
//   Arduino Library Manager : busque "AM_MIDI2.0Lib"
//   PlatformIO              : lib_deps = midi2-dev/AM_MIDI2.0Lib
//
// Boards suportadas: ESP32-S2, ESP32-S3 (USB), qualquer ESP32 (BLE).
//
// Arduino IDE:
//   Tools > Board  → ESP32S3 Dev Module (USB) ou qualquer ESP32 (BLE)

// ---- 1. AM_MIDI2.0Lib PRIMEIRO (ordem obrigatória) ----------------------
#include <umpProcessor.h>
#include <bytestreamToUMP.h>

// ---- 2. ESP32_Host_MIDI + Adapter ----------------------------------------
#include <ESP32_Host_MIDI.h>
#include "../../src/MIDI2Adapter.h"

// ---- Instâncias -----------------------------------------------------------
umpProcessor umpp;           // processador AM_MIDI2.0Lib — callbacks aqui
MIDI2Adapter adapter(umpp);  // ponte entre midiHandler e umpp

// ---- Callbacks AM_MIDI2.0Lib ---------------------------------------------
//
// channelVoiceMessage — recebe TODOS os Channel Voice Messages em UMP:
//   Note On/Off, CC, Pitch Bend, Channel Pressure, Poly Pressure,
//   Program Change, RPN, NRPN, Per-Note Pitch Bend…
//
//   msg.messageType  : MT nibble (0x2 = MIDI1-in-UMP, 0x4 = MIDI2 CVM)
//   msg.status       : opcode (0x8=NoteOff, 0x9=NoteOn, 0xB=CC, 0xE=PB…)
//   msg.umpGroup     : grupo (0-15)
//   msg.channel      : canal (0-15, MIDI 2.0 usa 0-indexed)
//   msg.note         : nota/controller (0-127)
//   msg.value        : valor 32-bit ← O DIFERENCIAL MIDI 2.0
//                      NoteOn/Off  → upper 16-bit = velocity (0-65535)
//                      CC          → 32-bit value (0-4294967295)
//                      Pitch Bend  → 32-bit, centro = 0x80000000
//
void onChannelVoice(umpCVM msg) {
    // ---- Identificar mensagem por opcode ----
    switch (msg.status) {
        case 0x9:  // Note On
        {
            uint16_t vel16 = (uint16_t)(msg.value >> 16);  // 16-bit velocity
            uint8_t  vel7  = (uint8_t)(vel16 >> 9);        // escala para 7-bit
            Serial.printf("[AM-MIDI2] NoteOn  ch=%d  note=%d  vel7=%d  vel16=%u\n",
                          msg.channel + 1, msg.note, vel7, vel16);
            break;
        }
        case 0x8:  // Note Off
            Serial.printf("[AM-MIDI2] NoteOff ch=%d  note=%d\n",
                          msg.channel + 1, msg.note);
            break;
        case 0xB:  // Control Change
            Serial.printf("[AM-MIDI2] CC      ch=%d  ctrl=%d  val32=%u  val7=%d\n",
                          msg.channel + 1, msg.note, msg.value,
                          (int)(msg.value >> 25));
            break;
        case 0xE:  // Pitch Bend
        {
            int32_t pb = (int32_t)(msg.value - 0x80000000UL);
            Serial.printf("[AM-MIDI2] PB      ch=%d  val32=%+ld  val14=%+d\n",
                          msg.channel + 1, (long)pb,
                          (int)(msg.value >> 18) - 8192);
            break;
        }
        case 0xD:  // Channel Pressure
            Serial.printf("[AM-MIDI2] ChPress ch=%d  val32=%u\n",
                          msg.channel + 1, msg.value);
            break;
        case 0xA:  // Poly Pressure
            Serial.printf("[AM-MIDI2] PolyPrs ch=%d  note=%d  val32=%u\n",
                          msg.channel + 1, msg.note, msg.value);
            break;
        case 0xC:  // Program Change
            Serial.printf("[AM-MIDI2] PC      ch=%d  prog=%d\n",
                          msg.channel + 1, msg.note);
            break;
        default:
            Serial.printf("[AM-MIDI2] CVM     status=0x%X  ch=%d  val=%u\n",
                          msg.status, msg.channel + 1, msg.value);
    }
}

// systemMessage — Real-Time e System Common em UMP
void onSystemMessage(umpGeneric msg) {
    Serial.printf("[AM-MIDI2] System  status=0x%02X\n", msg.status);
}

// sendOutSysex — SysEx7 encapsulado em UMP (Type 3, 64-bit)
// Nota: SysEx bruto de MIDI 1.0 NÃO é encaminhado automaticamente
// pelo adapter (Phase 1). Use midiHandler.setSysExCallback() para
// acessar SysEx recebido via transports MIDI 1.0.
void onSysex(umpData msg) {
    Serial.printf("[AM-MIDI2] SysEx7  group=%d  len=%d\n",
                  msg.umpGroup, msg.dataLength);
}

// ---- Callback MIDI 1.0 legado (ESP32_Host_MIDI, acesso à fila) -----------
//
// Este callback é opcional. A fila midiHandler.getQueue() ainda funciona
// normalmente — o MIDI2Adapter NÃO quebra a API existente.
// Use-o quando precisar dos campos já processados (noteName, chordIndex, etc.)
//
void onMIDI1Event(const MIDIEventData& ev) {
    // Exemplo: acessar dados MIDI 1.0 ao mesmo tempo que o AM-MIDI2 entrega 32-bit
    Serial.printf("[MIDI 1.0] %-12s  ch=%d  note=%d  vel=%d  chord=%d\n",
                  ev.status.c_str(), ev.channel, ev.note,
                  ev.velocity, ev.chordIndex);
}

// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32_Host_MIDI + AM_MIDI2.0Lib ===");

    // ---- Registrar callbacks do AM_MIDI2.0Lib ----------------------------
    umpp.channelVoiceMessage = onChannelVoice;
    umpp.systemMessage       = onSystemMessage;
    umpp.sendOutSysex        = onSysex;

    // ---- Inicializar midiHandler -----------------------------------------
    MIDIHandlerConfig cfg;
    cfg.maxEvents    = 20;
    cfg.maxSysExSize = 256;
    midiHandler.begin(cfg);

    // ---- Conectar o adapter ----------------------------------------------
    // A partir daqui todo MIDI 1.0 recebido pelos transports USB/BLE/UART
    // será convertido para UMP e entregue ao umpp automaticamente.
    adapter.attachToHandler(midiHandler);

    // Opcional: definir o grupo UMP padrão (0-15).
    // Grupo 0 representa o primeiro bloco de 16 canais MIDI 2.0.
    adapter.setDefaultGroup(0);

    Serial.println("Pronto. Conecte um teclado MIDI por USB ou BLE.");
    Serial.println("Callbacks AM-MIDI2: channelVoiceMessage, systemMessage, sendOutSysex");
    Serial.println("API legada        : midiHandler.getQueue() continua funcionando");
}

// =========================================================================
void loop() {
    // midiHandler.task() recebe dados dos transports e chama o rawMidiCb
    // internamente, que por sua vez aciona o adapter → umpProcessor.
    midiHandler.task();

    // ---- API legada ESP32_Host_MIDI (opcional, ainda funciona normalmente) ---
    static int lastIdx = -1;
    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        if (ev.index <= lastIdx) continue;
        lastIdx = ev.index;
        onMIDI1Event(ev);
    }
}
