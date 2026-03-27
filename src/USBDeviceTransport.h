#ifndef USB_DEVICE_TRANSPORT_H
#define USB_DEVICE_TRANSPORT_H

#include <Arduino.h>
#include <vector>
#include <usb/usb_host.h>
#include <freertos/portmacro.h>
#include "MIDITransport.h"
#include "MIDITypes.h"

class USBDeviceTransport : public MIDITransport {
public:
    USBDeviceTransport();

    // Initialize this transport for a specific already-opened USB device.
    // Called by USBHubManager after device enumeration.
    // Returns true if a MIDI interface was found and claimed.
    bool attach(usb_host_client_handle_t client, usb_device_handle_t device, uint8_t address);

    // Release USB resources. Called by USBHubManager on disconnect.
    void detach();

    // MIDITransport interface
    void task() override;
    bool isConnected() const override { return _ready; }

    // Device identification
    uint8_t deviceAddress() const { return _address; }

    // Submit the IN transfer (called by USBHubManager's polling task)
    void submitTransfer();

    // Enqueue received data (called from transfer callback, ISR-safe)
    bool enqueuePacket(const uint8_t* data, size_t length);

private:
    bool _ready;
    uint8_t _address;
    uint8_t _interval;
    unsigned long _lastPoll;

    usb_host_client_handle_t _clientHandle;
    usb_device_handle_t _deviceHandle;
    usb_transfer_t* _transfer;
    uint8_t _claimedInterface;
    bool _interfaceClaimed;

    // Ring buffer (same proven pattern from USBConnection)
    static const int QUEUE_SIZE = 64;
    RawUsbMessage _queue[QUEUE_SIZE];
    volatile int _head;
    volatile int _tail;
    portMUX_TYPE _mux;

    // SysEx reassembly
    bool _sysexActive;
    std::vector<uint8_t> _sysexBuf;

    void processQueue();
    bool dequeue(RawUsbMessage& msg);
    bool parseConfig(const usb_config_desc_t* config);

    static void _onReceive(usb_transfer_t* transfer);
};

#endif // USB_DEVICE_TRANSPORT_H
