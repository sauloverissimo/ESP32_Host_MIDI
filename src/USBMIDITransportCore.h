#ifndef USB_MIDI_TRANSPORT_CORE_H
#define USB_MIDI_TRANSPORT_CORE_H

#include <cstdint>

// Pure USB MIDI host transport logic. No Arduino, no usb_host.h.
// Consumed by the real transport classes AND the native tests, so tests
// validate the real code (not copies).
namespace usbmidi { namespace core {

struct AltCandidate {
    uint8_t  ifaceNumber;
    uint8_t  altSetting;
    bool     isMIDI2;        // bcdMSC >= 0x0200
    uint8_t  epInAddress;    // IN endpoint (0 = none found)
    uint16_t epInMaxPacket;
    uint8_t  epInInterval;
    uint8_t  epOutAddress;   // OUT endpoint (0 = none found)
    uint16_t epOutMaxPacket;
};

// Scan a full configuration descriptor; prefer the MIDI 2.0 alt setting.
// Returns false if no MIDI Streaming interface with an IN endpoint exists.
inline bool findBestAlt(const uint8_t* p, uint16_t totalLen, AltCandidate& best) {
    AltCandidate midi1 = {}, midi2 = {};
    bool found1 = false, found2 = false;

    uint16_t i = 0;
    while (i < totalLen) {
        if (i + 1 >= totalLen) break;
        uint8_t dlen  = p[i];
        uint8_t dtype = p[i + 1];
        if (dlen < 2 || (i + dlen) > totalLen) break;

        // Interface descriptor (type 0x04, min 9 bytes)
        if (dtype == 0x04 && dlen >= 9) {
            uint8_t ifNum    = p[i + 2];
            uint8_t altSet   = p[i + 3];
            uint8_t numEP    = p[i + 4];
            uint8_t ifClass  = p[i + 5];
            uint8_t ifSubCls = p[i + 6];

            // Audio class (0x01), MIDI Streaming subclass (0x03)
            if (ifClass == 0x01 && ifSubCls == 0x03) {
                AltCandidate cand = {};
                cand.ifaceNumber = ifNum;
                cand.altSetting  = altSet;

                // Scan descriptors belonging to this alt setting
                uint16_t j = i + dlen;
                while (j < totalLen) {
                    if (j + 1 >= totalLen) break;
                    uint8_t blen  = p[j];
                    uint8_t btype = p[j + 1];
                    if (blen < 2 || (j + blen) > totalLen) break;
                    if (btype == 0x04) break;  // next interface descriptor

                    // CS_INTERFACE (0x24), MS Header subtype (0x01), min 5 bytes
                    if (btype == 0x24 && blen >= 5 && p[j + 2] == 0x01) {
                        uint16_t bcdMSC = p[j + 3] | ((uint16_t)p[j + 4] << 8);
                        cand.isMIDI2 = (bcdMSC >= 0x0200);
                    }

                    // Standard endpoint descriptor (0x05), min 7 bytes
                    if (btype == 0x05 && blen >= 7 && numEP > 0) {
                        uint8_t  epAddr = p[j + 2];
                        uint16_t maxPkt = p[j + 4] | ((uint16_t)p[j + 5] << 8);
                        uint8_t  bInt   = p[j + 6];
                        if (maxPkt == 0)   maxPkt = 64;
                        if (maxPkt > 512)  maxPkt = 512;
                        if (epAddr & 0x80) {   // IN endpoint
                            cand.epInAddress   = epAddr;
                            cand.epInMaxPacket = maxPkt;
                            cand.epInInterval  = bInt ? bInt : 1;
                        } else {               // OUT endpoint
                            cand.epOutAddress   = epAddr;
                            cand.epOutMaxPacket = maxPkt;
                        }
                    }

                    j += blen;
                }

                if (cand.epInAddress != 0) {
                    if (cand.isMIDI2) { midi2 = cand; found2 = true; }
                    else               { midi1 = cand; found1 = true; }
                }
            }
        }

        i += dlen;
    }

    if (found2) { best = midi2; return true; }
    if (found1) { best = midi1; return true; }
    return false;
}

// UMP words-per-packet by message type (high nibble).
inline uint8_t umpWordCount(uint8_t mt) {
    switch (mt) {
        case 0x0: case 0x1: case 0x2: case 0x6: case 0x7: return 1;
        case 0x3: case 0x4: case 0x8: case 0x9: case 0xA: return 2;
        case 0xB: case 0xC: return 3;
        default: return 4; // 0x5, 0xD, 0xE, 0xF
    }
}

}} // namespace usbmidi::core

#endif // USB_MIDI_TRANSPORT_CORE_H
