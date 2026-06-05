// ESP32_Host_MIDI / AM-MIDI2-Adapter
// Bridge MIDI 1.0 (USB or BLE) into AM_MIDI2.0Lib's umpProcessor as UMP.
//
// Receives MIDI 1.0 over USB or BLE, converts it to UMP (Universal MIDI
// Packet), and delivers it to umpProcessor with MIDI 2.0 high-resolution
// values. Prints both the MIDI 1.0 (compatibility) and MIDI 2.0 (high-res)
// views on Serial.
//
// Requires: AM_MIDI2.0Lib (Arduino Library Manager: "AM_MIDI2.0Lib";
// PlatformIO: midi2-dev/AM_MIDI2.0Lib).
// Arduino IDE: Board ESP32-S3 (USB) or any ESP32 (BLE) | Serial 115200

// ---- 1. AM_MIDI2.0Lib first (required include order) ---------------------
#include <umpProcessor.h>
#include <bytestreamToUMP.h>

// ---- 2. ESP32_Host_MIDI + Adapter ----------------------------------------
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>
#include <MIDI2Adapter.h>

// ---- Instances -----------------------------------------------------------
USBConnection usbHost;
umpProcessor umpp;           // AM_MIDI2.0Lib processor; callbacks registered here
MIDI2Adapter adapter(umpp);  // bridge between midiHandler and umpp

// ---- AM_MIDI2.0Lib callbacks ---------------------------------------------
//
// channelVoiceMessage receives every Channel Voice Message in UMP form:
//   Note On/Off, CC, Pitch Bend, Channel Pressure, Poly Pressure,
//   Program Change, RPN, NRPN, Per-Note Pitch Bend...
//
//   msg.messageType : MT nibble (0x2 = MIDI1-in-UMP, 0x4 = MIDI2 CVM)
//   msg.status      : opcode (0x8=NoteOff, 0x9=NoteOn, 0xB=CC, 0xE=PB...)
//   msg.umpGroup    : group (0-15)
//   msg.channel     : channel (0-15, MIDI 2.0 is 0-indexed)
//   msg.note        : note / controller (0-127)
//   msg.value       : 32-bit value (the MIDI 2.0 high-resolution field)
//                     NoteOn/Off  -> upper 16-bit = velocity (0-65535)
//                     CC          -> 32-bit value
//                     Pitch Bend  -> 32-bit, center = 0x80000000
//
void onChannelVoice(umpCVM msg) {
    switch (msg.status) {
        case 0x9:  // Note On
        {
            uint16_t vel16 = (uint16_t)(msg.value >> 16);  // 16-bit velocity
            uint8_t  vel7  = (uint8_t)(vel16 >> 9);        // scaled to 7-bit
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

// systemMessage: Real-Time and System Common in UMP form.
void onSystemMessage(umpGeneric msg) {
    Serial.printf("[AM-MIDI2] System  status=0x%02X\n", msg.status);
}

// sendOutSysex: SysEx7 wrapped in UMP (Type 3, 64-bit).
// Note: raw MIDI 1.0 SysEx is NOT auto-forwarded by the adapter (Phase 1).
// Use midiHandler.setSysExCallback() to access SysEx received over MIDI 1.0
// transports.
void onSysex(umpData msg) {
    Serial.printf("[AM-MIDI2] SysEx7  group=%d  len=%d\n",
                  msg.umpGroup, msg.dataLength);
}

// ---- Legacy MIDI 1.0 callback (ESP32_Host_MIDI queue access) --------------
//
// Optional. midiHandler.getQueue() still works normally; the MIDI2Adapter
// does not break the existing API. Use this when you need the already-parsed
// fields (note name, chord index, etc.).
//
void onMIDI1Event(const MIDIEventData& ev) {
    Serial.printf("[MIDI 1.0] %-12s  ch=%d  note=%d  vel=%d  chord=%d\n",
                  MIDIHandler::statusName(ev.statusCode), ev.channel0, ev.noteNumber,
                  ev.velocity7, ev.chordIndex);
}

// =========================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32_Host_MIDI + AM_MIDI2.0Lib ===");

    // ---- Register AM_MIDI2.0Lib callbacks --------------------------------
    umpp.channelVoiceMessage = onChannelVoice;
    umpp.systemMessage       = onSystemMessage;
    umpp.sendOutSysex        = onSysex;

    // ---- Initialize midiHandler ------------------------------------------
    MIDIHandlerConfig cfg;
    cfg.maxEvents    = 20;
    cfg.maxSysExSize = 256;
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin(cfg);

    // ---- Attach the adapter ----------------------------------------------
    // From here on, all MIDI 1.0 received over the USB/BLE/UART transports is
    // converted to UMP and delivered to umpp automatically.
    adapter.attachToHandler(midiHandler);

    // Optional: set the default UMP group (0-15). Group 0 is the first block
    // of 16 MIDI 2.0 channels.
    adapter.setDefaultGroup(0);

    Serial.println("Ready. Connect a MIDI keyboard over USB or BLE.");
    Serial.println("AM-MIDI2 callbacks: channelVoiceMessage, systemMessage, sendOutSysex");
    Serial.println("Legacy API        : midiHandler.getQueue() still works");
}

// =========================================================================
void loop() {
    // midiHandler.task() receives data from the transports and calls the raw
    // MIDI callback internally, which drives the adapter -> umpProcessor.
    midiHandler.task();

    // ---- Legacy ESP32_Host_MIDI API (optional, still works normally) -----
    static int lastIdx = -1;
    const auto& queue = midiHandler.getQueue();
    for (const auto& ev : queue) {
        if (ev.index <= lastIdx) continue;
        lastIdx = ev.index;
        onMIDI1Event(ev);
    }
}
