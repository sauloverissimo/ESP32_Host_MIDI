#ifndef MIDI2_ADAPTER_H
#define MIDI2_ADAPTER_H

// MIDI2Adapter — bridge between ESP32_Host_MIDI transports and AM_MIDI2.0Lib
//
// Standard library: AM_MIDI2.0Lib (https://github.com/midi2-dev/AM_MIDI2.0Lib)
// Officially endorsed by the MIDI Association. MIT License.
// Arduino/ESP32 compatible (C++11, ~10 KB compiled, ~1 KB RAM).
//
// ---- Prerequisite -------------------------------------------------------
// Install AM_MIDI2.0Lib via Arduino Library Manager or PlatformIO:
//   lib_deps = midi2-dev/AM_MIDI2.0Lib
//
// Include AM_MIDI2.0Lib headers BEFORE this file:
//   #include <umpProcessor.h>
//   #include <bytestreamToUMP.h>
//   #include "../../src/MIDI2Adapter.h"
//
// ---- Two input paths ----------------------------------------------------
//
//  1. MIDI 1.0 transports (USB, BLE, UART, ESP-NOW…)
//     Call attachToHandler(midiHandler) once in setup().
//     All incoming MIDI 1.0 data is automatically converted to UMP and
//     forwarded to umpProcessor callbacks.
//
//  2. MIDI 2.0 UMP transports
//     Some transports expose high-resolution UMP data via a lastResult()
//     accessor after scaling down to MIDI 1.0 for dispatch. Use
//     feedFromUMPResult() to inject the original UMP into umpProcessor:
//
//       const UMPResult& r = midi2Transport.lastResult();
//       if (r.valid) adapter.feedFromUMPResult(r);
//
// ---- Basic usage --------------------------------------------------------
//
//   #include <umpProcessor.h>
//   #include <bytestreamToUMP.h>
//   #include "../../src/MIDI2Adapter.h"
//   #include "../../src/ESP32_Host_MIDI.h"
//
//   umpProcessor umpp;
//   MIDI2Adapter adapter(umpp);
//
//   void setup() {
//       umpp.channelVoiceMessage = [](umpCVM msg) {
//           // msg.channel (0-15), msg.note (0-127)
//           // msg.value = 32-bit high-res value  ← the MIDI 2.0 difference
//       };
//       umpp.sendOutSysex = [](umpData msg) { /* SysEx7 (Type 3) in UMP */ };
//
//       midiHandler.begin();
//       adapter.attachToHandler(midiHandler);  // wire up automatically
//   }
//
//   void loop() {
//       midiHandler.task();  // internally calls rawMidiCb → adapter → umpp
//   }
//
// ---- Notes --------------------------------------------------------------
//   - Only one MIDI2Adapter can be attached to a MIDIHandler at a time
//   - Full MIDI 2.0 (Flex Data, Stream Messages, MIDI-CI): future phases
//
// ---- Spec references ----------------------------------------------------
//   MIDI 2.0 Specification M2-104-UM   — midi.org
//   Universal MIDI Packet Format       — midi.org/specifications

#include "MIDI2Support.h"
#include "MIDIHandler.h"

class MIDI2Adapter {
public:
    // Constructor — pass your AM_MIDI2.0Lib umpProcessor by reference.
    explicit MIDI2Adapter(umpProcessor& proc) : _proc(proc) {
        _instance = this;
        _b2ump.defaultGroup = 0;
    }

    // ---- MIDI 1.0 channel-voice path ------------------------------------

    // Convert raw MIDI 1.0 bytes to UMP and forward to umpProcessor.
    // Status byte and data bytes only (no CIN byte, no SysEx).
    // Called automatically via rawMidiCb when attachToHandler() is used.
    void feedMIDI1(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            _b2ump.bytestreamParse(data[i]);
        }
        while (_b2ump.availableUMP()) {
            _proc.processUMP(_b2ump.readUMP());
        }
    }

    // ---- SysEx7 path (Phase 2) ------------------------------------------

    // Convert a complete MIDI 1.0 SysEx message to SysEx7 UMP packets (Type 3)
    // and forward each 64-bit packet to umpProcessor.
    //
    // data    : full SysEx bytes including 0xF0 and 0xF7 markers.
    // len     : total byte count (must be ≥ 2, first byte 0xF0, last 0xF7).
    //
    // The payload (bytes between 0xF0 and 0xF7) is split into 6-byte chunks:
    //   ≤6 bytes → one Complete packet (SYSEX7_COMPLETE)
    //   >6 bytes → Start + N×Continue + End packets
    //
    // Called automatically by the sysExCb trampoline when attachToHandler() is used.
    // umpProcessor delivers the reassembled chunks via its sendOutSysex callback:
    //   umpp.sendOutSysex = [](umpData msg) { ... };
    void feedSysEx1(const uint8_t* data, size_t len) {
        if (!data || len < 2 || data[0] != 0xF0) return;
        // Strip 0xF0 and 0xF7 — feed only the payload
        const uint8_t* payload = data + 1;
        size_t payloadLen = (data[len - 1] == 0xF7) ? len - 2 : len - 1;
        _feedSysEx7Stream(_b2ump.defaultGroup, payload, payloadLen);
    }

    // ---- Direct UMP path ------------------------------------------------

    // Feed a single 32-bit UMP word (Type 0, 1 or 2 — single-word packet).
    void feedUMPWord(uint32_t word) {
        _proc.processUMP(word);
    }

    // Feed a 64-bit UMP packet (two 32-bit words — Type 3 or 4).
    void feedUMP64(uint32_t word0, uint32_t word1) {
        _proc.processUMP(word0);
        _proc.processUMP(word1);
    }

    // Feed a 128-bit UMP packet (four 32-bit words — Type 5 or F).
    void feedUMP128(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
        _proc.processUMP(w0);
        _proc.processUMP(w1);
        _proc.processUMP(w2);
        _proc.processUMP(w3);
    }

    // Reconstruct and feed a UMP packet from a UMPResult.
    // Use this when a MIDI 2.0 transport exposes high-resolution values
    // via a lastResult() accessor after dispatching scaled-down MIDI 1.0:
    //
    //   midiHandler.task();
    //   const UMPResult& r = midi2Transport.lastResult();
    //   if (r.valid) adapter.feedFromUMPResult(r);
    void feedFromUMPResult(const UMPResult& r) {
        if (!r.valid) return;

        if (r.isMIDI2) {
            // Reconstruct Type 4 (MIDI 2.0 Channel Voice, 64-bit)
            // word0: [MT=4][Group][Opcode][Channel][Note/Index][AttrType=0]
            uint32_t w0 = ((uint32_t)UMP_MT_MIDI2_VOICE << 28) |
                          ((uint32_t)(r.group   & 0x0F) << 24) |
                          ((uint32_t)(r.opcode  & 0x0F) << 20) |
                          ((uint32_t)(r.channel & 0x0F) << 16) |
                          ((uint32_t)(r.note    & 0x7F) <<  8);
            // word1: 32-bit value (velocity upper 16-bit, CC value, pitch bend…)
            _proc.processUMP(w0);
            _proc.processUMP(r.value);
        } else {
            // Reconstruct Type 2 (MIDI 1.0 in UMP, 32-bit) from stored bytes
            if (r.midi1Len > 0) {
                UMPWord32 pkt = UMPBuilder::fromMIDI1(r.group, r.midi1, r.midi1Len);
                _proc.processUMP(pkt.raw);
            }
        }
    }

    // ---- MIDIHandler integration ----------------------------------------

    // Hook this adapter into MIDIHandler via rawMidiCb and sysExCb.
    // After this call:
    //   • All incoming MIDI 1.0 channel-voice data → converted to UMP → umpProcessor
    //   • All incoming SysEx messages → split into SysEx7 UMP packets → umpProcessor
    // Call once in setup(), after midiHandler.begin().
    // Note: only one MIDI2Adapter per MIDIHandler at a time.
    void attachToHandler(MIDIHandler& handler) {
        _instance = this;
        handler.setRawMidiCallback(_staticRawCallback);
        handler.setSysExCallback(_staticSysExCallback);
    }

    // Remove this adapter from MIDIHandler.
    void detachFromHandler(MIDIHandler& handler) {
        handler.setRawMidiCallback(nullptr);
        handler.setSysExCallback(nullptr);
        if (_instance == this) _instance = nullptr;
    }

    // Set the default UMP group for MIDI 1.0 → UMP conversion (0-15).
    // Group 0 (default) maps to the first 16-channel MIDI 2.0 block.
    void setDefaultGroup(uint8_t group) {
        _b2ump.defaultGroup = group & 0x0F;
    }

    // Direct access to the underlying umpProcessor (for dynamic callback assignment).
    umpProcessor& processor() { return _proc; }

    // Static instance pointer — required for the rawMidiCb trampoline.
    inline static MIDI2Adapter* _instance = nullptr;

private:
    umpProcessor&   _proc;
    bytestreamToUMP _b2ump;

    // Returns the expected byte length of a MIDI 1.0 message from its status byte.
    static size_t _midi1MsgLen(uint8_t status) {
        switch (status & 0xF0) {
            case 0x80: case 0x90: case 0xA0:
            case 0xB0: case 0xE0: return 3;
            case 0xC0: case 0xD0: return 2;
            default:              return 1;
        }
    }

    // Segment a raw SysEx payload (without 0xF0/0xF7) into SysEx7 UMP packets
    // and feed each pair of words to umpProcessor.
    void _feedSysEx7Stream(uint8_t group, const uint8_t* payload, size_t len) {
        if (len == 0) {
            // Empty SysEx — send a Complete packet with 0 payload bytes
            UMPWord64 pkt = UMPBuilder::sysEx7(group, SYSEX7_COMPLETE, nullptr, 0);
            _proc.processUMP(pkt.word0);
            _proc.processUMP(pkt.word1);
            return;
        }
        size_t offset = 0;
        bool   first  = true;
        while (offset < len) {
            size_t  remaining = len - offset;
            uint8_t chunk     = (remaining > 6) ? 6 : (uint8_t)remaining;
            uint8_t status;
            if (len <= 6) {
                status = SYSEX7_COMPLETE;
            } else if (first) {
                status = SYSEX7_START;
            } else if (remaining <= 6) {
                status = SYSEX7_END;
            } else {
                status = SYSEX7_CONTINUE;
            }
            UMPWord64 pkt = UMPBuilder::sysEx7(group, status, payload + offset, chunk);
            _proc.processUMP(pkt.word0);
            _proc.processUMP(pkt.word1);
            offset += chunk;
            first   = false;
        }
    }

    // Static trampoline — signature matches MIDIHandler::RawMidiCallback.
    // Called by MIDIHandler::handleMidiMessage() before internal parsing.
    // midi3 points to the MIDI status byte (USB CIN byte already stripped by handler).
    static void _staticRawCallback(const uint8_t* /*raw*/, size_t rawLen,
                                   const uint8_t* midi3) {
        if (!_instance || !midi3 || rawLen < 2) return;
        uint8_t status = midi3[0];
        // SysEx is handled via _staticSysExCallback — skip here.
        if (status == 0xF0 || (status & 0xF8) == 0xF0) return;
        size_t midiLen = _midi1MsgLen(status);
        _instance->feedMIDI1(midi3, midiLen);
    }

    // Static trampoline — signature matches MIDIHandler::SysExCallback.
    // Called by MIDIHandler::handleSysExMessage() for each complete SysEx received.
    static void _staticSysExCallback(const uint8_t* data, size_t len) {
        if (_instance) _instance->feedSysEx1(data, len);
    }
};

#endif // MIDI2_ADAPTER_H
