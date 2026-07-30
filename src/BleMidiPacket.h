#ifndef BLE_MIDI_PACKET_H
#define BLE_MIDI_PACKET_H

#include <Arduino.h>

// Splits a BLE-MIDI notification/write payload into individual MIDI messages.
//
// BLE-MIDI framing (Apple / MMA spec): a packet is
//   [header] [timestamp] [event] ( [timestamp] [event] )*
// where header and every timestamp byte have bit 7 set, and running status lets
// an event omit its status byte. A single packet can therefore carry several
// MIDI messages, so stripping only the first two bytes and forwarding the rest
// as one blob misparses batched notifications.
//
// This splitter walks the payload by position (a timestamp precedes each event)
// and calls sink(ctx, msg, len) once per complete channel-voice or system
// message. Running status is carried across events.
//
// SysEx (0xF0) spans are not reframed here: on reaching one, the remainder of
// the packet is emitted as a single blob, which matches the previous behavior
// for single-packet SysEx and avoids corrupting it. Truncated tails are dropped.
static inline void bleMidiSplitMessages(
        const uint8_t* pkt, size_t len,
        void (*sink)(void* ctx, const uint8_t* msg, size_t msgLen), void* ctx) {
    // Too short to hold header + timestamp + one MIDI byte: nothing to emit.
    if (len < 3) return;

    size_t i = 1;          // skip the header byte
    uint8_t status = 0;    // running status

    while (i < len) {
        // A timestamp byte (bit 7 set) precedes each event. Skip it.
        if (pkt[i] & 0x80) {
            i++;
            if (i >= len) break;
        }

        // Status byte (bit 7 set) or running-status data (bit 7 clear).
        if (pkt[i] & 0x80) {
            status = pkt[i];
            if (status == 0xF0) {
                // SysEx: hand the rest of the packet over untouched.
                sink(ctx, pkt + i, len - i);
                return;
            }
            i++;
        }
        if (status == 0) break;   // running status with no status yet

        // Data-byte count for this status.
        int need;
        uint8_t hi = status & 0xF0;
        if (hi == 0xC0 || hi == 0xD0) {
            need = 1;                      // Program Change, Channel Pressure
        } else if (hi >= 0x80 && hi <= 0xE0) {
            need = 2;                      // Note Off/On, Poly Key, CC, Pitch Bend
        } else if (status == 0xF1 || status == 0xF3) {
            need = 1;                      // MTC Quarter Frame, Song Select
        } else if (status == 0xF2) {
            need = 2;                      // Song Position Pointer
        } else {
            need = 0;                      // 0xF6, real-time (0xF8..0xFF)
        }

        uint8_t msg[3];
        msg[0] = status;
        int n = 0;
        while (n < need && i < len && !(pkt[i] & 0x80)) {
            msg[1 + n] = pkt[i];
            i++;
            n++;
        }
        if (n == need) {
            sink(ctx, msg, (size_t)(1 + need));
        } else {
            break;   // truncated event
        }
    }
}

#endif // BLE_MIDI_PACKET_H
