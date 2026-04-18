#include "USBDeviceTransport.h"
#include <string.h>
#include <esp_log.h>

static const char* TAG = "USBMidiDev";

USBDeviceTransport::USBDeviceTransport()
  : _ready(false),
    _address(0),
    _interval(1),
    _lastPoll(0),
    _clientHandle(nullptr),
    _deviceHandle(nullptr),
    _transfer(nullptr),
    _outTransfer(nullptr),
    _outEndpoint(0),
    _outMaxPacketSize(0),
    _claimedInterface(0),
    _interfaceClaimed(false),
    _head(0),
    _tail(0),
    _mux(portMUX_INITIALIZER_UNLOCKED),
    _sysexActive(false)
{
}

bool USBDeviceTransport::attach(usb_host_client_handle_t client, usb_device_handle_t device, uint8_t address) {
    _clientHandle = client;
    _deviceHandle = device;
    _address = address;

    const usb_config_desc_t* config_desc;
    esp_err_t err = usb_host_get_active_config_descriptor(_deviceHandle, &config_desc);
    if (err != ESP_OK) {
        return false;
    }

    if (!parseConfig(config_desc)) {
        return false;
    }

    // Initial transfer submit
    if (_ready && _transfer) {
        usb_host_transfer_submit(_transfer);
    }

    return _ready;
}

void USBDeviceTransport::detach() {
    _ready = false;

    if (_transfer) {
        usb_host_transfer_free(_transfer);
        _transfer = nullptr;
    }

    if (_outTransfer) {
        usb_host_transfer_free(_outTransfer);
        _outTransfer = nullptr;
    }
    _outEndpoint = 0;
    _outMaxPacketSize = 0;

    if (_interfaceClaimed) {
        usb_host_interface_release(_clientHandle, _deviceHandle, _claimedInterface);
        _interfaceClaimed = false;
    }

    // Reset queue
    portENTER_CRITICAL(&_mux);
    _head = 0;
    _tail = 0;
    portEXIT_CRITICAL(&_mux);

    // Reset SysEx state
    _sysexActive = false;
    _sysexBuf.clear();

    _clientHandle = nullptr;
    _deviceHandle = nullptr;
    _address = 0;
}

void USBDeviceTransport::task() {
    processQueue();
}

void USBDeviceTransport::submitTransfer() {
    if (_ready && _transfer) {
        usb_host_transfer_submit(_transfer);
    }
}

bool USBDeviceTransport::enqueuePacket(const uint8_t* data, size_t length) {
    portENTER_CRITICAL(&_mux);
    int next = (_head + 1) % QUEUE_SIZE;
    if (next == _tail) {
        // Queue full; discard the message.
        portEXIT_CRITICAL(&_mux);
        return false;
    }
    size_t copyLength = 4;
    memcpy(_queue[_head].data, data, copyLength);
    _queue[_head].length = copyLength;
    _head = next;
    portEXIT_CRITICAL(&_mux);
    return true;
}

bool USBDeviceTransport::dequeue(RawUsbMessage& msg) {
    portENTER_CRITICAL(&_mux);
    if (_tail == _head) {
        portEXIT_CRITICAL(&_mux);
        return false;
    }
    msg = _queue[_tail];
    _tail = (_tail + 1) % QUEUE_SIZE;
    portEXIT_CRITICAL(&_mux);
    return true;
}

void USBDeviceTransport::processQueue() {
    RawUsbMessage msg;
    while (dequeue(msg)) {
        if (msg.length < 4) {
            dispatchMidiData(msg.data, msg.length);
            continue;
        }
        uint8_t cin = msg.data[0] & 0x0F;

        switch (cin) {
            case 0x04:  // SysEx start or continue (3 data bytes)
                if (!_sysexActive) {
                    _sysexBuf.clear();
                    _sysexActive = true;
                }
                _sysexBuf.push_back(msg.data[1]);
                _sysexBuf.push_back(msg.data[2]);
                _sysexBuf.push_back(msg.data[3]);
                break;

            case 0x05:  // SysEx end — 1 data byte
                if (_sysexActive) {
                    _sysexBuf.push_back(msg.data[1]);
                    dispatchSysExData(_sysexBuf.data(), _sysexBuf.size());
                    _sysexActive = false;
                    _sysexBuf.clear();
                }
                break;

            case 0x06:  // SysEx end — 2 data bytes
                if (_sysexActive) {
                    _sysexBuf.push_back(msg.data[1]);
                    _sysexBuf.push_back(msg.data[2]);
                    dispatchSysExData(_sysexBuf.data(), _sysexBuf.size());
                    _sysexActive = false;
                    _sysexBuf.clear();
                }
                break;

            case 0x07:  // SysEx end — 3 data bytes
                if (_sysexActive) {
                    _sysexBuf.push_back(msg.data[1]);
                    _sysexBuf.push_back(msg.data[2]);
                    _sysexBuf.push_back(msg.data[3]);
                    dispatchSysExData(_sysexBuf.data(), _sysexBuf.size());
                    _sysexActive = false;
                    _sysexBuf.clear();
                }
                break;

            default:
                // Non-SysEx message — abort any incomplete SysEx and dispatch normally
                if (_sysexActive) { _sysexActive = false; _sysexBuf.clear(); }
                dispatchMidiData(msg.data, msg.length);
                break;
        }
    }
}

int USBDeviceTransport::scanMidiInterfaces(
        const usb_config_desc_t* config_desc,
        IntfCandidate candidates[MAX_CANDIDATES])
{
    const uint8_t* p = config_desc->val;
    uint16_t totalLength = config_desc->wTotalLength;
    uint16_t index = 0;
    int count = 0;

    while (index < totalLength) {
        if (index + 1 >= totalLength) break;
        uint8_t len = p[index];
        if (len < 2 || (index + len) > totalLength) break;

        uint8_t descriptorType = p[index + 1];
        if (descriptorType == 0x04 && len >= 9) {       // Interface
            uint8_t bIntfNum      = p[index + 2];
            uint8_t bAltSetting   = p[index + 3];
            uint8_t bNumEps       = p[index + 4];
            uint8_t bIntfClass    = p[index + 5];
            uint8_t bIntfSubClass = p[index + 6];

            ESP_LOGD(TAG, "intf %d alt %d cls 0x%02x sub 0x%02x eps %d",
                     bIntfNum, bAltSetting, bIntfClass, bIntfSubClass, bNumEps);

            if (bIntfClass == 0x01 && bIntfSubClass == 0x03 && bNumEps > 0) {
                if (count >= MAX_CANDIDATES) {
                    ESP_LOGW(TAG, "candidate list full at %d, truncating scan",
                             MAX_CANDIDATES);
                    break;
                }
                candidates[count] = { bIntfNum, bAltSetting, bNumEps,
                                      (uint16_t)(index + len) };
                count++;
            }
        }
        index += len;
    }
    return count;
}

int USBDeviceTransport::selectBestCandidate(const IntfCandidate* cand, int count)
{
    if (count == 0) return -1;

    // Pass 1: pick the target interface number -- the FIRST one that appears
    // in descriptor order. This matches the legacy behavior of parseConfig
    // which claimed the first MIDI Streaming interface it found.
    uint8_t targetIntf = cand[0].bInterfaceNumber;

    // Pass 2: among all candidates for that interface, prefer the highest
    // alt setting (Alt 1 > Alt 0) for MIDI 2.0 preference.
    int bestIdx = -1;
    uint8_t bestAlt = 0;
    for (int i = 0; i < count; i++) {
        if (cand[i].bInterfaceNumber != targetIntf) continue;
        if (bestIdx < 0 || cand[i].bAlternateSetting > bestAlt) {
            bestIdx = i;
            bestAlt = cand[i].bAlternateSetting;
        }
    }
    return bestIdx;
}

bool USBDeviceTransport::parseConfig(const usb_config_desc_t* config_desc) {
    // Reset per-attach state (this object may be reused across reconnects)
    _outEndpoint = 0;
    _outMaxPacketSize = 0;

    IntfCandidate candidates[MAX_CANDIDATES];
    int n = scanMidiInterfaces(config_desc, candidates);
    if (n == 0) {
        ESP_LOGW(TAG, "no MIDI Streaming interface found (addr %d)", _address);
        return false;
    }

    int best = selectBestCandidate(candidates, n);
    if (best < 0) {
        ESP_LOGW(TAG, "no viable MIDI interface candidate (addr %d)", _address);
        return false;
    }

    const IntfCandidate& c = candidates[best];
    ESP_LOGI(TAG, "selected MIDI intf %d alt %d (from %d candidates)",
             c.bInterfaceNumber, c.bAlternateSetting, n);

    esp_err_t err = usb_host_interface_claim(
        _clientHandle, _deviceHandle,
        c.bInterfaceNumber, c.bAlternateSetting);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "claim intf %d alt %d failed: 0x%x",
                 c.bInterfaceNumber, c.bAlternateSetting, err);
        return false;
    }

    _claimedInterface = c.bInterfaceNumber;
    _interfaceClaimed = true;

    // Walk endpoints from recorded offset. Stop at next interface descriptor.
    const uint8_t* p = config_desc->val;
    uint16_t totalLength = config_desc->wTotalLength;
    uint16_t idx = c.endpointsOffset;
    bool foundIn = false;

    while (idx < totalLength) {
        if (idx + 1 >= totalLength) break;
        uint8_t len = p[idx];
        if (len < 2 || (idx + len) > totalLength) break;
        uint8_t type = p[idx + 1];
        if (type == 0x04) break;  // next interface
        if (type == 0x05 && len >= 7) {
            uint8_t bEpAddr   = p[idx + 2];
            uint8_t bmAttr    = p[idx + 3];
            uint16_t wMaxPkt  = (p[idx + 4] | (p[idx + 5] << 8));
            uint8_t bInterval = p[idx + 6];
            if (wMaxPkt > 512) wMaxPkt = 512;
            if (wMaxPkt == 0) wMaxPkt = 64;

            ESP_LOGD(TAG, "  ep 0x%02x attr 0x%02x pkt %d",
                     bEpAddr, bmAttr, wMaxPkt);

            if (bEpAddr & 0x80) {
                // IN endpoint (device -> host)
                if (!foundIn) {
                    uint8_t xferType = bmAttr & 0x03;
                    uint32_t timeout = (xferType == 0x02) ? 3000 : 0;
                    esp_err_t e2 = usb_host_transfer_alloc(wMaxPkt, timeout, &_transfer);
                    if (e2 == ESP_OK && _transfer) {
                        _transfer->device_handle    = _deviceHandle;
                        _transfer->bEndpointAddress = bEpAddr;
                        _transfer->callback         = _onReceive;
                        _transfer->context          = this;
                        _transfer->num_bytes        = wMaxPkt;
                        _interval = (bInterval == 0) ? 1 : bInterval;
                        foundIn = true;
                    }
                }
            } else {
                // OUT endpoint (host -> device) -- first OUT wins
                if (_outEndpoint == 0) {
                    _outEndpoint = bEpAddr;
                    _outMaxPacketSize = wMaxPkt;
                }
            }
        }
        idx += len;
    }

    if (!foundIn) {
        // NOTE: behavioral change vs legacy parseConfig.
        // Legacy code would advance the outer loop and try the next MIDI
        // Streaming interface descriptor on this failure. New code picks a
        // single candidate up front and gives up if it fails. This is a
        // conscious choice: malformed preferred alt settings are rare, and
        // the simplicity of a single claim path outweighs the edge case.
        // If field reports show devices hitting this path, revisit.
        ESP_LOGW(TAG, "intf %d alt %d claimed but no IN endpoint found",
                 c.bInterfaceNumber, c.bAlternateSetting);
        usb_host_interface_release(_clientHandle, _deviceHandle, c.bInterfaceNumber);
        _interfaceClaimed = false;
        _claimedInterface = 0;
        return false;
    }

    if (_outEndpoint != 0) {
        usb_host_transfer_alloc(_outMaxPacketSize, 0, &_outTransfer);
        if (_outTransfer) {
            _outTransfer->device_handle    = _deviceHandle;
            _outTransfer->bEndpointAddress = _outEndpoint;
            _outTransfer->callback         = _onSendComplete;
            _outTransfer->context          = this;
        }
    }

    ESP_LOGI(TAG, "MIDI intf %d alt %d claimed (IN 0x%02x OUT 0x%02x)",
             c.bInterfaceNumber, c.bAlternateSetting,
             _transfer ? _transfer->bEndpointAddress : 0, _outEndpoint);
    _ready = true;
    return true;
}

bool USBDeviceTransport::sendMidiMessage(const uint8_t* data, size_t length) {
    if (!_ready || !_outTransfer || _outEndpoint == 0 || length < 1) {
        return false;
    }

    // Build USB-MIDI event packet (4 bytes per message).
    // Cable Number = 0, Code Index Number derived from status byte.
    uint8_t status = data[0] & 0xF0;
    uint8_t cin = 0;

    switch (status) {
        case 0x80: cin = 0x08; break;  // Note Off
        case 0x90: cin = 0x09; break;  // Note On
        case 0xA0: cin = 0x0A; break;  // Poly Pressure
        case 0xB0: cin = 0x0B; break;  // Control Change
        case 0xC0: cin = 0x0C; break;  // Program Change
        case 0xD0: cin = 0x0D; break;  // Channel Pressure
        case 0xE0: cin = 0x0E; break;  // Pitch Bend
        default: return false;          // SysEx and others not handled here
    }

    _outTransfer->num_bytes = 4;
    _outTransfer->data_buffer[0] = cin;
    _outTransfer->data_buffer[1] = data[0];
    _outTransfer->data_buffer[2] = (length > 1) ? data[1] : 0;
    _outTransfer->data_buffer[3] = (length > 2) ? data[2] : 0;

    esp_err_t err = usb_host_transfer_submit(_outTransfer);
    return (err == ESP_OK);
}

void USBDeviceTransport::_onSendComplete(usb_transfer_t* transfer) {
    // Nothing to do -- transfer completed. The buffer is reusable.
    (void)transfer;
}

void USBDeviceTransport::_onReceive(usb_transfer_t* transfer) {
    USBDeviceTransport* transport = static_cast<USBDeviceTransport*>(transfer->context);
    if (transfer->status == 0 && transfer->actual_num_bytes >= 4) {
        // Iterate in 4-byte blocks (each block = 1 USB-MIDI event)
        for (int offset = 0; offset + 4 <= transfer->actual_num_bytes; offset += 4) {
            if (transfer->data_buffer[offset] == 0x00) continue;
            transport->enqueuePacket(transfer->data_buffer + offset, 4);
        }
    }
    if (transport->_ready) {
        esp_err_t err = usb_host_transfer_submit(transfer);
        (void)err;
    }
}
