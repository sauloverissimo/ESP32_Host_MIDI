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

// ── UMP reassembly across bulk transfers ────────────────────────────────────
//
// USB MIDI 2.0 (Alt 1) delivers raw UMP words over the bulk IN endpoint. A UMP
// packet (1..4 words) may be split across two transfers. umpReassemble() holds
// the partial tail in a carry buffer and prepends it to the next transfer, so
// no word is lost at a transfer boundary.

// Max UMP words a single bulk-IN transfer can carry (512-byte clamp / 4).
static const uint16_t UMP_MAX_TRANSFER_WORDS = 128;
// Worst-case complete words emitted in one call = up to 3 carried words that
// complete alongside a full transfer of single-word packets. out[] must be this big.
static const uint16_t UMP_OUT_WORDS = 3 + UMP_MAX_TRANSFER_WORDS;

struct UMPCarry { uint32_t words[3]; uint8_t count; }; // max partial = 3 words

// Prepend carry, emit complete packets to out[], store the trailing partial back
// into carry. Returns the number of words written to out[]. Never reads past
// inCount, never writes past outMax, never drops a word.
inline uint16_t umpReassemble(const uint32_t* in, uint16_t inCount,
                              UMPCarry& carry, uint32_t* out, uint16_t outMax) {
    uint32_t buf[3 + UMP_MAX_TRANSFER_WORDS];
    const uint16_t cap = (uint16_t)(sizeof(buf) / sizeof(buf[0]));
    uint16_t n = 0;
    for (uint8_t i = 0; i < carry.count && n < cap; ++i) buf[n++] = carry.words[i];
    for (uint16_t i = 0; i < inCount && n < cap; ++i)    buf[n++] = in[i];
    carry.count = 0;

    uint16_t i = 0, w = 0;
    while (i < n) {
        uint8_t mt = (buf[i] >> 28) & 0x0F;
        uint8_t pw = umpWordCount(mt);
        if (i + pw > n) {                 // partial tail -> carry
            uint8_t rem = (uint8_t)(n - i);
            for (uint8_t k = 0; k < rem && k < 3; ++k) carry.words[k] = buf[i + k];
            carry.count = rem;
            break;
        }
        for (uint8_t k = 0; k < pw && w < outMax; ++k) out[w++] = buf[i + k];
        i += pw;
    }
    return w;
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

// ── UMP Stream Messages (MT 0x0F) ───────────────────────────────────────────

static const uint8_t  UMP_MT_STREAM             = 0x0F;
static const uint16_t STREAM_ENDPOINT_DISCOVERY = 0x000;
static const uint16_t STREAM_ENDPOINT_INFO      = 0x001;
static const uint16_t STREAM_DEVICE_INFO        = 0x002;
static const uint16_t STREAM_EP_NAME            = 0x003;
static const uint16_t STREAM_PROD_INSTANCE_ID   = 0x004;
static const uint16_t STREAM_CONFIG_REQUEST     = 0x005;
static const uint16_t STREAM_CONFIG_NOTIFY      = 0x006;
static const uint16_t STREAM_FB_DISCOVERY       = 0x010;
static const uint16_t STREAM_FB_INFO            = 0x011;
static const uint16_t STREAM_FB_NAME            = 0x012;

// Discovery message builders (fill a 4-word UMP Stream message).
inline void buildEndpointDiscovery(uint32_t msg[4], uint8_t verMajor, uint8_t verMinor) {
    msg[0] = ((uint32_t)UMP_MT_STREAM << 28) | ((uint32_t)STREAM_ENDPOINT_DISCOVERY << 16)
           | ((uint32_t)verMajor << 8) | (uint32_t)verMinor;
    msg[1] = 0xFF;  // filter: request all info
    msg[2] = 0; msg[3] = 0;
}
inline void buildFunctionBlockDiscovery(uint32_t msg[4]) {
    msg[0] = ((uint32_t)UMP_MT_STREAM << 28) | ((uint32_t)STREAM_FB_DISCOVERY << 16)
           | (0xFF << 8)   // fbIdx = 0xFF (request all)
           | 0xFF;         // filter = all info
    msg[1] = 0; msg[2] = 0; msg[3] = 0;
}

// ── Negotiation state machine ───────────────────────────────────────────────
//
// Pure model of the UMP endpoint-discovery handshake. The transport layer feeds
// each incoming Stream Message to negStep() and performs the returned action.

enum class NegState : uint8_t {
    Idle, AwaitEndpointInfo, AwaitFBInfo, Done
};
enum class NegAction : uint8_t {
    None, SendFBDiscovery, Complete
};
struct NegEngine {
    NegState state = NegState::Idle;
    uint8_t  numFunctionBlocks = 0;
    bool     supportsMIDI2 = false;
    uint8_t  currentProtocol = 0;  // informational; set only on an unsolicited 0x006
    uint8_t  fbCount = 0;
    uint8_t  fbExpected = 0;
};

// Feed one Stream Message (4 words). Updates the engine and returns the action
// the transport layer must perform (send a packet / complete / nothing).
//
// Read-only discovery only: on Endpoint Info the host goes straight to Function
// Block Discovery (no Stream Config Request, which would command a protocol
// switch). A Stream Config Notify (0x006) may still arrive unsolicited; it is
// recorded but never gates progress.
inline NegAction negStep(NegEngine& e, const uint32_t* words) {
    uint16_t status = (words[0] >> 16) & 0x3FF;
    switch (status) {
    case STREAM_ENDPOINT_INFO:
        e.numFunctionBlocks = (words[1] >> 24) & 0xFF;
        e.supportsMIDI2     = (words[1] >> 9) & 0x01;
        if (e.state == NegState::AwaitEndpointInfo) {
            e.fbCount = 0;
            e.fbExpected = e.numFunctionBlocks;
            if (e.fbExpected > 0) {
                e.state = NegState::AwaitFBInfo;
                return NegAction::SendFBDiscovery;
            }
            e.state = NegState::Done;
            return NegAction::Complete;
        }
        break;
    case STREAM_CONFIG_NOTIFY:
        // Unsolicited protocol notification: record, do not gate progress.
        e.currentProtocol = (words[0] >> 8) & 0xFF;
        break;
    case STREAM_FB_INFO:
        if (e.state == NegState::AwaitFBInfo) {
            e.fbCount++;
            if (e.fbCount >= e.fbExpected) {
                e.state = NegState::Done;
                return NegAction::Complete;
            }
        }
        break;
    default:
        break;
    }
    return NegAction::None;
}

// True for MT 0x0F stream messages the host consumes internally: the defined
// discovery/config statuses (0x000..0x006 and 0x010..0x012) plus the reserved
// gap (0x007..0x00F), all treated as host-internal so none reaches the user UMP
// callback as if it were music. Defined statuses: Endpoint Discovery/Info,
// Device Info, EP Name, Product Id, Stream Config Req/Notify, FB Disc/Info/Name.
inline bool isInternalStreamMessage(const uint32_t* words) {
    if (((words[0] >> 28) & 0x0F) != UMP_MT_STREAM) return false;
    uint16_t status = (words[0] >> 16) & 0x3FF;
    return status <= STREAM_FB_NAME;
}

}} // namespace usbmidi::core

#endif // USB_MIDI_TRANSPORT_CORE_H
