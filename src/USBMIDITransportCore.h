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

// ── Group Terminal Blocks ───────────────────────────────────────────────────

static const uint8_t GTB_HEADER_SUBTYPE = 0x01;
static const uint8_t GTB_BLOCK_SUBTYPE  = 0x02;
static const uint8_t GTB_BLOCK_DESC_LEN = 13;

struct GTBlock {
    uint8_t  id;
    uint8_t  type;           // 0=bidirectional, 1=input-only, 2=output-only
    uint8_t  firstGroup;
    uint8_t  numGroups;
    uint8_t  protocol;       // 0x01=MIDI1, 0x02=MIDI2, 0x00=unknown
    uint16_t maxInputBW;
    uint16_t maxOutputBW;
};

// Parse a GTB GET_DESCRIPTOR response body (header + N block entries).
// Fills out[] up to maxOut and returns the number of blocks parsed.
inline uint8_t parseGTB(const uint8_t* data, uint16_t len, GTBlock* out, uint8_t maxOut) {
    // GTB Header: bLength(1) + bDescriptorType(1) + bDescriptorSubtype(1) + wTotalLength(2)
    if (len < 5) return 0;
    if (data[2] != GTB_HEADER_SUBTYPE) return 0;

    uint16_t totalLen = data[3] | ((uint16_t)data[4] << 8);
    if (totalLen > len) totalLen = len;

    uint16_t offset = data[0];  // skip header (bLength bytes)
    uint8_t count = 0;

    while (offset + GTB_BLOCK_DESC_LEN <= totalLen && count < maxOut) {
        const uint8_t* blk = data + offset;
        if (blk[0] < GTB_BLOCK_DESC_LEN) break;
        if (blk[2] != GTB_BLOCK_SUBTYPE) { offset += blk[0]; continue; }

        GTBlock& g = out[count];
        g.id          = blk[3];
        g.type        = blk[4];
        g.firstGroup  = blk[5];
        g.numGroups   = blk[6];
        // blk[7] = iBlockItem (string descriptor index — skip)
        g.protocol    = blk[8];
        g.maxInputBW  = blk[9]  | ((uint16_t)blk[10] << 8);
        g.maxOutputBW = blk[11] | ((uint16_t)blk[12] << 8);
        count++;

        offset += blk[0];
    }
    return count;
}

// ── Stream text accumulation (Endpoint Name, Product Instance ID) ────────────
//
// UMP Stream text messages use a form field: 0=complete, 1=start, 2=continue,
// 3=end. Text bytes are packed in word0[15:8], word0[7:0], then word1..word3.
inline void appendStreamText(char* dest, uint8_t& destLen, uint8_t maxLen,
                             const uint32_t* words, uint8_t form) {
    // Start or Complete resets the buffer
    if (form == 0 || form == 1) {
        destLen = 0;
    }

    uint8_t text[14];
    uint8_t n = 0;
    uint8_t b;

    b = (words[0] >> 8) & 0xFF; if (b) text[n++] = b;
    b = words[0] & 0xFF;        if (b) text[n++] = b;
    for (uint8_t w = 1; w <= 3; w++) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            b = (words[w] >> shift) & 0xFF;
            if (b) text[n++] = b;
        }
    }

    for (uint8_t i = 0; i < n && destLen < maxLen; i++) {
        dest[destLen++] = (char)text[i];
    }
    dest[destLen] = '\0';
}

}} // namespace usbmidi::core

#endif // USB_MIDI_TRANSPORT_CORE_H
