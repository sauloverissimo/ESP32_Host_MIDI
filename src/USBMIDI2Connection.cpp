#include "USBMIDI2Connection.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// UMP Stream Message constants (MT 0x0F)
static const uint8_t  UMP_MT_STREAM            = 0x0F;
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

// GTB descriptor type (USB class-specific)
static const uint8_t CS_GR_TRM_BLOCK = 0x26;
static const uint8_t GTB_HEADER_SUBTYPE = 0x01;
static const uint8_t GTB_BLOCK_SUBTYPE  = 0x02;
static const uint8_t GTB_BLOCK_DESC_LEN = 13;

// UMP version we report as Host
static const uint8_t UMP_VER_MAJOR = 1;
static const uint8_t UMP_VER_MINOR = 1;

// Timeout for each negotiation step (ms)
static const unsigned long NEG_TIMEOUT_MS = 2000;

// ── Constructor ──────────────────────────────────────────────────────────────

USBMIDI2Connection::USBMIDI2Connection()
  : USBConnection(),
    _midi2Active(false)
{
}

// ── _processConfig — override with Alt 1 detection ──────────────────────────

void USBMIDI2Connection::_processConfig(const usb_config_desc_t* config_desc) {
    AltCandidate best;
    memset(&best, 0, sizeof(best));

    if (!_findBestAlt(config_desc->val, config_desc->wTotalLength, best)) {
        USBConnection::_processConfig(config_desc);
        return;
    }

    if (!_claimAndSetup(best)) {
        return;
    }

    _midi2Active = best.isMIDI2;

    if (_midi2Active) {
        _readGTBDescriptors();
        if (_outTransfer) {
            _startNegotiation();
        }
    }
}

// ── _onDeviceGone — free OUT transfer on disconnect ──────────────────────────

void USBMIDI2Connection::_onDeviceGone() {
    if (_outTransfer) {
        usb_host_transfer_free(_outTransfer);
        _outTransfer = nullptr;
    }
    _midi2Active = false;
    _negotiationState = NegIdle;
    memset(&_epInfo, 0, sizeof(_epInfo));
    _fbCount = 0;
    _fbExpected = 0;
    _gtbCount = 0;
    _epName[0] = '\0'; _epNameLen = 0;
    _prodId[0] = '\0'; _prodIdLen = 0;
}

// ── _findBestAlt — single-pass scan, two candidates ─────────────────────────

bool USBMIDI2Connection::_findBestAlt(const uint8_t* p, uint16_t totalLen,
                                      AltCandidate& best) {
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

// ── _claimAndSetup ──────────────────────────────────────────────────────────

bool USBMIDI2Connection::_claimAndSetup(const AltCandidate& cand) {
    esp_err_t err = usb_host_interface_claim(clientHandle, deviceHandle,
                                             cand.ifaceNumber, cand.altSetting);
    if (err != ESP_OK) {
        lastError = "MIDI2: claim failed (iface=" + String(cand.ifaceNumber)
                  + " alt=" + String(cand.altSetting) + " err=" + String(err) + ")";
        return false;
    }

    // Force-resend SET_INTERFACE via EP0 control transfer.
    //
    // Per ESP-IDF docs, usb_host_interface_claim already issues SET_INTERFACE
    // when the requested alt differs from the active one. In practice some
    // device combinations (observed with at least one MIDI 2.0 device built
    // on the TinyUSB MIDI 2.0 PR #3571 class driver) silently stay on Alt 0
    // (USB-MIDI 1.0 CIN) while the host believes it has switched to Alt 1
    // (USB-MIDI 2.0 UMP). The result: bulk IN transfers come back populated
    // with CIN packets that _onReceiveUMP misinterprets as UMP words with
    // reserved MT nibbles.
    //
    // Sending the request explicitly closes the gap. SET_INTERFACE on FS USB
    // is a single 8-byte SETUP with no data stage; under 1 ms in practice.
    // We wait up to 200 ms for the transfer callback before freeing the
    // transfer to avoid use-after-free, then wait another 50 ms for the
    // device-side class driver to fully repoint its TX path before we start
    // submitting bulk reads.
    {
        static SemaphoreHandle_t s_setifSem = nullptr;
        if (s_setifSem == nullptr) s_setifSem = xSemaphoreCreateBinary();

        usb_transfer_t* setifT = nullptr;
        if (usb_host_transfer_alloc(8, 0, &setifT) == ESP_OK && setifT != nullptr) {
            setifT->device_handle    = deviceHandle;
            setifT->bEndpointAddress = 0x00;
            setifT->num_bytes        = 8;
            setifT->timeout_ms       = 200;
            setifT->context          = (void*)s_setifSem;
            setifT->callback         = [](usb_transfer_t* t) {
                SemaphoreHandle_t sem = (SemaphoreHandle_t)t->context;
                if (sem) xSemaphoreGive(sem);
            };
            // SETUP: bmRequestType=0x01 (host->device, standard, interface),
            // bRequest=0x0B (SET_INTERFACE), wValue=alt, wIndex=iface, wLength=0.
            setifT->data_buffer[0] = 0x01;
            setifT->data_buffer[1] = 0x0B;
            setifT->data_buffer[2] = cand.altSetting;
            setifT->data_buffer[3] = 0x00;
            setifT->data_buffer[4] = cand.ifaceNumber;
            setifT->data_buffer[5] = 0x00;
            setifT->data_buffer[6] = 0x00;
            setifT->data_buffer[7] = 0x00;

            if (usb_host_transfer_submit_control(clientHandle, setifT) == ESP_OK) {
                xSemaphoreTake(s_setifSem, pdMS_TO_TICKS(200));
            }
            usb_host_transfer_free(setifT);
        }
        // Settle window: allow the device-side class driver to repoint its
        // bulk TX path to the new alt before we start reading.
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Allocate IN transfer (receive)
    err = usb_host_transfer_alloc(cand.epInMaxPacket, 3000, &midiTransfer);
    if (err != ESP_OK || midiTransfer == nullptr) {
        usb_host_interface_release(clientHandle, deviceHandle, cand.ifaceNumber);
        lastError = "MIDI2: IN transfer alloc failed";
        return false;
    }

    midiTransfer->device_handle    = deviceHandle;
    midiTransfer->bEndpointAddress = cand.epInAddress;
    midiTransfer->num_bytes        = cand.epInMaxPacket;
    midiTransfer->context          = this;
    midiTransfer->callback         = cand.isMIDI2 ? _onReceiveUMP : _onReceive;

    // Allocate OUT transfer (send) if OUT endpoint exists
    if (cand.epOutAddress != 0) {
        err = usb_host_transfer_alloc(cand.epOutMaxPacket, 3000, &_outTransfer);
        if (err != ESP_OK || _outTransfer == nullptr) {
            usb_host_transfer_free(midiTransfer);
            midiTransfer = nullptr;
            usb_host_interface_release(clientHandle, deviceHandle, cand.ifaceNumber);
            lastError = "MIDI2: OUT transfer alloc failed";
            return false;
        }
        _outTransfer->device_handle    = deviceHandle;
        _outTransfer->bEndpointAddress = cand.epOutAddress;
        _outTransfer->num_bytes        = 0;
        _outTransfer->context          = this;
        _outTransfer->callback         = _onSendComplete;
    }

    _claimedIfaceNumber = cand.ifaceNumber;
    interval = cand.epInInterval;
    isReady  = true;
    return true;
}

// ── Protocol Negotiation — UMP Stream Messages (MT 0x0F) ────────────────────
//
// After selecting Alt 1, the Host performs:
//   1. Send Endpoint Discovery Request → device responds with Endpoint Info
//   2. Send Stream Config Request (protocol=0x02) → device confirms
//   3. Send Function Block Discovery → device responds with FB Info(s)
//
// All stream messages are 128-bit (4 words). The state machine advances
// as responses arrive in _onReceiveUMP → _processStreamMessage.

void USBMIDI2Connection::_startNegotiation() {
    _negotiationState = NegAwaitEndpointInfo;
    _negTimeout = millis() + NEG_TIMEOUT_MS;
    _sendEndpointDiscovery();
}

void USBMIDI2Connection::retryNegotiation() {
    if (!isReady || !_midi2Active || _negotiationState == NegDone)
        return;
    _startNegotiation();
}

void USBMIDI2Connection::_sendEndpointDiscovery() {
    // MT=0xF, status=0x000 (Endpoint Discovery), UMP version, filter=0xFF (all)
    uint32_t msg[4] = {};
    msg[0] = ((uint32_t)UMP_MT_STREAM << 28)
           | ((uint32_t)STREAM_ENDPOINT_DISCOVERY << 16)
           | ((uint32_t)UMP_VER_MAJOR << 8)
           | (uint32_t)UMP_VER_MINOR;
    msg[1] = 0xFF;  // filter: request all info
    sendUMPMessage(msg, 4);
}

void USBMIDI2Connection::_sendStreamConfigRequest(uint8_t protocol) {
    // MT=0xF, status=0x005 (Stream Config Request), protocol in byte 2
    uint32_t msg[4] = {};
    msg[0] = ((uint32_t)UMP_MT_STREAM << 28)
           | ((uint32_t)STREAM_CONFIG_REQUEST << 16)
           | ((uint32_t)protocol << 8);
    sendUMPMessage(msg, 4);
}

void USBMIDI2Connection::_sendFunctionBlockDiscovery() {
    // MT=0xF, status=0x010 (FB Discovery), fbIdx=0xFF (all), filter=0xFF (all)
    uint32_t msg[4] = {};
    msg[0] = ((uint32_t)UMP_MT_STREAM << 28)
           | ((uint32_t)STREAM_FB_DISCOVERY << 16)
           | (0xFF << 8)   // fbIdx = 0xFF (request all)
           | 0xFF;         // filter = all info
    sendUMPMessage(msg, 4);
}

// ── _processStreamMessage — handle MT 0x0F responses from device ────────────

void USBMIDI2Connection::_processStreamMessage(const uint32_t* words) {
    uint16_t status = (words[0] >> 16) & 0x3FF;

    switch (status) {

    case STREAM_ENDPOINT_INFO:
        // Endpoint Info Notification
        _epInfo.umpVersionMajor      = (words[0] >> 8) & 0xFF;
        _epInfo.umpVersionMinor      = words[0] & 0xFF;
        _epInfo.numFunctionBlocks    = (words[1] >> 24) & 0xFF;
        _epInfo.supportsMIDI2Protocol = (words[1] >> 9) & 0x01;
        _epInfo.supportsMIDI1Protocol = (words[1] >> 8) & 0x01;
        _epInfo.supportsRxJR         = (words[1] >> 1) & 0x01;
        _epInfo.supportsTxJR         = words[1] & 0x01;

        if (_negotiationState == NegAwaitEndpointInfo) {
            // Request MIDI 2.0 Protocol if supported, else MIDI 1.0
            uint8_t proto = _epInfo.supportsMIDI2Protocol ? 0x02 : 0x01;
            _negotiationState = NegAwaitStreamConfig;
            _negTimeout = millis() + NEG_TIMEOUT_MS;
            _sendStreamConfigRequest(proto);
        }
        break;

    case STREAM_DEVICE_INFO:
        // Device Identity Notification — stored for future use
        break;

    case STREAM_EP_NAME: {
        uint8_t form = (words[0] >> 26) & 0x03;
        _appendStreamText(_epName, _epNameLen, MAX_NAME_LEN - 1, words, form);
        break;
    }

    case STREAM_PROD_INSTANCE_ID: {
        uint8_t form = (words[0] >> 26) & 0x03;
        _appendStreamText(_prodId, _prodIdLen, MAX_NAME_LEN - 1, words, form);
        break;
    }

    case STREAM_CONFIG_NOTIFY:
        // Stream Configuration Notification
        _epInfo.currentProtocol = (words[0] >> 8) & 0xFF;

        if (_negotiationState == NegAwaitStreamConfig) {
            // Proceed to Function Block discovery
            _fbCount = 0;
            _fbExpected = _epInfo.numFunctionBlocks;
            if (_fbExpected > 0) {
                _negotiationState = NegAwaitFBInfo;
                _negTimeout = millis() + NEG_TIMEOUT_MS;
                _sendFunctionBlockDiscovery();
            } else {
                _negotiationState = NegDone;
            }
        }
        break;

    case STREAM_FB_INFO: {
        // Function Block Info Notification
        if (_fbCount < MAX_FUNCTION_BLOCKS) {
            FunctionBlockInfo& fb = _fbInfo[_fbCount];
            fb.index           = (words[0] >> 8) & 0x7F;
            fb.active          = (words[0] >> 15) & 0x01;
            fb.direction       = words[0] & 0x03;
            fb.firstGroup      = (words[1] >> 24) & 0xFF;
            fb.groupLength     = (words[1] >> 16) & 0xFF;
            fb.midiCISupport   = (words[1] >> 8) & 0xFF;
            fb.isMIDI1         = (words[0] >> 2) & 0x03;
            fb.maxSysEx8Streams = words[1] & 0xFF;
            _fbCount++;
        }

        if (_negotiationState == NegAwaitFBInfo) {
            if (_fbCount >= _fbExpected) {
                _negotiationState = NegDone;
            }
        }
        break;
    }

    case STREAM_FB_NAME:
        // Function Block Name — not stored currently (would need per-FB string array)
        break;

    default:
        break;
    }
}

// ── _onReceiveUMP — read raw UMP words from bulk IN ─────────────────────────
//
// USB MIDI 2.0 (Alt 1) sends raw UMP words over the bulk endpoint.
// No CIN header — each 4-byte block is one uint32_t UMP word.
// ESP32 is little-endian, and USB transfers are also little-endian,
// so we can read the words directly with no byte-swap.

void USBMIDI2Connection::_onReceiveUMP(usb_transfer_t* transfer) {
    USBMIDI2Connection* self = static_cast<USBMIDI2Connection*>(transfer->context);

    if (transfer->status == 0 && transfer->actual_num_bytes >= 4) {
        const uint32_t* words = reinterpret_cast<const uint32_t*>(transfer->data_buffer);
        uint8_t count = transfer->actual_num_bytes / 4;

        // Process UMP words — intercept Stream Messages (MT 0x0F) for negotiation,
        // dispatch everything else to user callback.
        uint8_t i = 0;
        while (i < count) {
            uint8_t mt = (words[i] >> 28) & 0x0F;

            // Determine packet size in words based on message type
            uint8_t pktWords;
            switch (mt) {
                case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
                    pktWords = 1; break;
                case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
                    pktWords = 2; break;
                case 0xB: case 0xC:
                    pktWords = 3; break;
                default:  // 0x5, 0xD, 0xE, 0xF = 128-bit
                    pktWords = 4; break;
            }

            if (i + pktWords > count) break;  // incomplete packet

            if (mt == 0x0F && pktWords == 4) {
                // Stream Message — handle internally for negotiation
                self->_processStreamMessage(&words[i]);
            }

            // Dispatch all UMP words to user (including stream messages)
            self->dispatchUMPData(&words[i], pktWords);
            i += pktWords;
        }
    }

    if (self->isReady) {
        usb_host_transfer_submit(transfer);
    }
}

// ── sendUMPMessage — write raw UMP words to device via OUT endpoint ─────────
//
// Each UMP word is 4 bytes, little-endian on the wire (matches ESP32 native).
// Maximum payload = OUT endpoint's wMaxPacketSize.

bool USBMIDI2Connection::sendUMPMessage(const uint32_t* words, uint8_t count) {
    if (!isReady || !_outTransfer || count == 0)
        return false;

    size_t byteLen = (size_t)count * 4;
    uint16_t maxPkt = _outTransfer->data_buffer_size;
    if (byteLen > maxPkt)
        return false;

    memcpy(_outTransfer->data_buffer, words, byteLen);
    _outTransfer->num_bytes = byteLen;

    esp_err_t err = usb_host_transfer_submit(_outTransfer);
    return (err == ESP_OK);
}

// ── _onSendComplete — OUT transfer completion ───────────────────────────────

void USBMIDI2Connection::_onSendComplete(usb_transfer_t* transfer) {
    (void)transfer;
}

// ── _appendStreamText — accumulate text from multi-packet stream messages ───
//
// UMP Stream text messages (Endpoint Name, Product Instance ID) use a form
// field to indicate single/start/continue/end packets:
//   form=0: complete (single packet)
//   form=1: start
//   form=2: continue
//   form=3: end
//
// Text bytes are packed in word0[15:8], word0[7:0], word1[31:0], word2[31:0],
// word3[31:0] — up to 14 bytes per packet.

void USBMIDI2Connection::_appendStreamText(char* dest, uint8_t& destLen,
                                           uint8_t maxLen,
                                           const uint32_t* words, uint8_t form) {
    // Start or Complete resets the buffer
    if (form == 0 || form == 1) {
        destLen = 0;
    }

    // Extract text bytes from the 128-bit packet
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

    // Append to destination
    for (uint8_t i = 0; i < n && destLen < maxLen; i++) {
        dest[destLen++] = (char)text[i];
    }
    dest[destLen] = '\0';
}

// ── GTB — Group Terminal Block descriptors (USB class-specific request) ─────
//
// GTB descriptors are read via a USB control transfer (GET_DESCRIPTOR)
// after the interface is claimed with Alt 1.
//
// Request:
//   bmRequestType = 0xA1 (class-specific, interface, device-to-host)
//   bRequest      = 0x06 (GET_DESCRIPTOR)
//   wValue        = (CS_GR_TRM_BLOCK << 8) | altSetting
//   wIndex        = interface number
//   wLength       = max response size
//
// Response: GTB Header (5 bytes) + N × GTB Block entries (13 bytes each)

void USBMIDI2Connection::_readGTBDescriptors() {
    // Allocate a temporary control transfer
    usb_transfer_t* ctrl = nullptr;
    // Control transfers need 8 bytes setup + data. Max GTB = 5 + 8*13 = 109 bytes
    esp_err_t err = usb_host_transfer_alloc(8 + 128, 3000, &ctrl);
    if (err != ESP_OK || ctrl == nullptr) return;

    // Build the setup packet in the first 8 bytes
    uint8_t* setup = ctrl->data_buffer;
    setup[0] = 0xA1;  // bmRequestType: class, interface, IN
    setup[1] = 0x06;  // bRequest: GET_DESCRIPTOR
    setup[2] = 0x01;  // wValue low: alt setting (1 for MIDI 2.0)
    setup[3] = CS_GR_TRM_BLOCK;  // wValue high: descriptor type
    setup[4] = _claimedIfaceNumber;  // wIndex low: interface
    setup[5] = 0x00;  // wIndex high
    setup[6] = 128;   // wLength low
    setup[7] = 0x00;  // wLength high

    ctrl->device_handle = deviceHandle;
    ctrl->bEndpointAddress = 0x00;  // control endpoint
    ctrl->num_bytes = 8 + 128;
    ctrl->context = this;
    ctrl->callback = _onGTBResponse;

    err = usb_host_transfer_submit_control(clientHandle, ctrl);
    if (err != ESP_OK) {
        usb_host_transfer_free(ctrl);
    }
    // ctrl is freed in the callback
}

void USBMIDI2Connection::_onGTBResponse(usb_transfer_t* transfer) {
    USBMIDI2Connection* self = static_cast<USBMIDI2Connection*>(transfer->context);

    if (transfer->status == 0 && transfer->actual_num_bytes > 8) {
        // Data starts after the 8-byte setup packet
        const uint8_t* data = transfer->data_buffer + 8;
        uint16_t dataLen = transfer->actual_num_bytes - 8;
        self->_parseGTBResponse(data, dataLen);
    }

    usb_host_transfer_free(transfer);
}

void USBMIDI2Connection::_parseGTBResponse(const uint8_t* data, uint16_t len) {
    // GTB Header: bLength(1) + bDescriptorType(1) + bDescriptorSubtype(1) + wTotalLength(2)
    if (len < 5) return;
    if (data[2] != GTB_HEADER_SUBTYPE) return;

    uint16_t totalLen = data[3] | ((uint16_t)data[4] << 8);
    if (totalLen > len) totalLen = len;

    // Parse GTB block entries after the header
    uint16_t offset = data[0];  // skip header (bLength bytes)
    _gtbCount = 0;

    while (offset + GTB_BLOCK_DESC_LEN <= totalLen && _gtbCount < MAX_GTB) {
        const uint8_t* blk = data + offset;
        if (blk[0] < GTB_BLOCK_DESC_LEN) break;
        if (blk[2] != GTB_BLOCK_SUBTYPE) { offset += blk[0]; continue; }

        GroupTerminalBlock& g = _gtb[_gtbCount];
        g.id          = blk[3];
        g.type        = blk[4];
        g.firstGroup  = blk[5];
        g.numGroups   = blk[6];
        // blk[7] = iBlockItem (string descriptor index — skip)
        g.protocol    = blk[8];
        g.maxInputBW  = blk[9]  | ((uint16_t)blk[10] << 8);
        g.maxOutputBW = blk[11] | ((uint16_t)blk[12] << 8);
        _gtbCount++;

        offset += blk[0];
    }
}
