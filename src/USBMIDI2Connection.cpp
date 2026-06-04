#include "USBMIDI2Connection.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// UMP Stream Message constants, builders and the negotiation state machine live
// in USBMIDITransportCore.h.
using namespace usbmidi::core;

// GTB descriptor type (USB class-specific). GTB parsing constants live in
// USBMIDITransportCore.h alongside parseGTB().
static const uint8_t CS_GR_TRM_BLOCK = 0x26;

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

    if (!usbmidi::core::findBestAlt(config_desc->val, config_desc->wTotalLength, best)) {
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
    _neg = NegEngine{};
    memset(&_epInfo, 0, sizeof(_epInfo));
    _fbCount = 0;
    _gtbCount = 0;
    _epName[0] = '\0'; _epNameLen = 0;
    _prodId[0] = '\0'; _prodIdLen = 0;
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
    _neg = NegEngine{};
    _neg.state = NegState::AwaitEndpointInfo;
    _negTimeout = millis() + NEG_TIMEOUT_MS;
    _sendEndpointDiscovery();
}

void USBMIDI2Connection::retryNegotiation() {
    if (!isReady || !_midi2Active || _neg.state == NegState::Done)
        return;
    _startNegotiation();
}

void USBMIDI2Connection::_sendEndpointDiscovery() {
    uint32_t msg[4];
    buildEndpointDiscovery(msg, UMP_VER_MAJOR, UMP_VER_MINOR);
    sendUMPMessage(msg, 4);
}

void USBMIDI2Connection::_sendStreamConfigRequest(uint8_t protocol) {
    uint32_t msg[4];
    buildStreamConfigRequest(msg, protocol);
    sendUMPMessage(msg, 4);
}

void USBMIDI2Connection::_sendFunctionBlockDiscovery() {
    uint32_t msg[4];
    buildFunctionBlockDiscovery(msg);
    sendUMPMessage(msg, 4);
}

// ── _processStreamMessage — handle MT 0x0F responses from device ────────────
//
// Populates the public info structs from each notification, then advances the
// pure negotiation state machine (negStep) and performs the returned action.

void USBMIDI2Connection::_processStreamMessage(const uint32_t* words) {
    uint16_t status = (words[0] >> 16) & 0x3FF;

    switch (status) {

    case STREAM_ENDPOINT_INFO:
        _epInfo.umpVersionMajor       = (words[0] >> 8) & 0xFF;
        _epInfo.umpVersionMinor       = words[0] & 0xFF;
        _epInfo.numFunctionBlocks     = (words[1] >> 24) & 0xFF;
        _epInfo.supportsMIDI2Protocol = (words[1] >> 9) & 0x01;
        _epInfo.supportsMIDI1Protocol = (words[1] >> 8) & 0x01;
        _epInfo.supportsRxJR          = (words[1] >> 1) & 0x01;
        _epInfo.supportsTxJR          = words[1] & 0x01;
        break;

    case STREAM_CONFIG_NOTIFY:
        _epInfo.currentProtocol = (words[0] >> 8) & 0xFF;
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

    case STREAM_FB_INFO:
        if (_fbCount < MAX_FUNCTION_BLOCKS) {
            FunctionBlockInfo& fb = _fbInfo[_fbCount];
            fb.index            = (words[0] >> 8) & 0x7F;
            fb.active           = (words[0] >> 15) & 0x01;
            fb.direction        = words[0] & 0x03;
            fb.firstGroup       = (words[1] >> 24) & 0xFF;
            fb.groupLength      = (words[1] >> 16) & 0xFF;
            fb.midiCISupport    = (words[1] >> 8) & 0xFF;
            fb.isMIDI1          = (words[0] >> 2) & 0x03;
            fb.maxSysEx8Streams = words[1] & 0xFF;
            _fbCount++;
        }
        break;

    default:
        break;
    }

    // Advance the negotiation state machine and perform the resulting action.
    switch (negStep(_neg, words)) {
    case NegAction::SendStreamConfigRequest:
        _negTimeout = millis() + NEG_TIMEOUT_MS;
        _sendStreamConfigRequest(_neg.configProtocol);
        break;
    case NegAction::SendFBDiscovery:
        _negTimeout = millis() + NEG_TIMEOUT_MS;
        _sendFunctionBlockDiscovery();
        break;
    case NegAction::None:
    case NegAction::Complete:
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
            uint8_t pktWords = usbmidi::core::umpWordCount(mt);

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
    usbmidi::core::appendStreamText(dest, destLen, maxLen, words, form);
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
    _gtbCount = usbmidi::core::parseGTB(data, len, _gtb, MAX_GTB);
}
