#include "USBDeviceTransport.h"
#include <string.h>

USBDeviceTransport::USBDeviceTransport()
  : _ready(false),
    _address(0),
    _interval(1),
    _lastPoll(0),
    _clientHandle(nullptr),
    _deviceHandle(nullptr),
    _transfer(nullptr),
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

bool USBDeviceTransport::parseConfig(const usb_config_desc_t* config_desc) {
    const uint8_t* p = config_desc->val;
    uint16_t totalLength = config_desc->wTotalLength;
    uint16_t index = 0;
    bool claimedOk = false;

    while (index < totalLength) {
        if (index + 1 >= totalLength) break;
        uint8_t len = p[index];
        if (len < 2 || (index + len) > totalLength) break;

        uint8_t descriptorType = p[index + 1];
        if (descriptorType == 0x04) { // Interface Descriptor
            if (len < 9) {
                index += len;
                continue;
            }
            uint8_t bInterfaceNumber   = p[index + 2];
            uint8_t bAlternateSetting  = p[index + 3];
            uint8_t bNumEndpoints      = p[index + 4];
            uint8_t bInterfaceClass    = p[index + 5];
            uint8_t bInterfaceSubClass = p[index + 6];
            if (bInterfaceClass == 0x01 && bInterfaceSubClass == 0x03) {
                esp_err_t err = usb_host_interface_claim(_clientHandle, _deviceHandle, bInterfaceNumber, bAlternateSetting);
                if (err == ESP_OK) {
                    _claimedInterface = bInterfaceNumber;
                    _interfaceClaimed = true;
                    uint16_t idx2 = index + len;
                    while (idx2 < totalLength) {
                        if (idx2 + 1 >= totalLength) break;
                        uint8_t len2 = p[idx2];
                        if (len2 < 2 || (idx2 + len2) > totalLength) break;
                        uint8_t type2 = p[idx2 + 1];
                        if (type2 == 0x04) break; // Next interface
                        if (type2 == 0x05 && bNumEndpoints > 0) {
                            if (len2 >= 7) {
                                uint8_t bEndpointAddress = p[idx2 + 2];
                                uint8_t bmAttributes = p[idx2 + 3];
                                uint16_t wMaxPacketSize = (p[idx2 + 4] | (p[idx2 + 5] << 8));
                                uint8_t bInterval = p[idx2 + 6];
                                if (wMaxPacketSize > 512) wMaxPacketSize = 512;
                                if (wMaxPacketSize == 0) wMaxPacketSize = 64;
                                if (bEndpointAddress & 0x80) {
                                    uint8_t transferType = bmAttributes & 0x03;
                                    uint32_t timeout = (transferType == 0x02) ? 3000 : 0;
                                    esp_err_t e2 = usb_host_transfer_alloc(wMaxPacketSize, timeout, &_transfer);
                                    if (e2 == ESP_OK && _transfer != nullptr) {
                                        _transfer->device_handle = _deviceHandle;
                                        _transfer->bEndpointAddress = bEndpointAddress;
                                        _transfer->callback = _onReceive;
                                        _transfer->context = this;
                                        _transfer->num_bytes = wMaxPacketSize;
                                        _interval = (bInterval == 0) ? 1 : bInterval;
                                        _ready = true;
                                        claimedOk = true;
                                        return true;
                                    }
                                }
                            }
                        }
                        idx2 += len2;
                    }
                    // No suitable endpoint found; release the interface
                    usb_host_interface_release(_clientHandle, _deviceHandle, bInterfaceNumber);
                    _interfaceClaimed = false;
                }
            }
        }
        index += len;
    }
    if (!claimedOk) {
        // Fallback: try a default endpoint with safe size
        esp_err_t err = usb_host_transfer_alloc(64, 3000, &_transfer);
        if (err == ESP_OK && _transfer != nullptr) {
            _transfer->device_handle = _deviceHandle;
            _transfer->bEndpointAddress = 0x81;
            _transfer->callback = _onReceive;
            _transfer->context = this;
            _transfer->num_bytes = 64;
            _interval = 1;
            _ready = true;
            return true;
        }
    }
    return false;
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
