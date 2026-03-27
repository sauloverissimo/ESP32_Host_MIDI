#ifndef USB_HUB_MANAGER_H
#define USB_HUB_MANAGER_H

#include <Arduino.h>
#include <usb/usb_host.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "USBDeviceTransport.h"

class MIDIHandler;  // forward declaration

class USBHubManager {
public:
    static constexpr int MAX_USB_DEVICES = 4;

    USBHubManager();
    ~USBHubManager();

    // Initialize USB Host stack, register client, start polling task.
    // handler: MIDIHandler to register discovered transports with.
    bool begin(MIDIHandler& handler);

    // Process pending transport add/remove operations.
    // MUST be called from main loop (same core as midiHandler.task()).
    void task();

    // Get number of currently connected MIDI devices
    int connectedDevices() const;

    // Access a specific device transport (nullptr if slot is empty)
    USBDeviceTransport* device(int index);

private:
    MIDIHandler* _handler;
    usb_host_client_handle_t _clientHandle;
    uint32_t _eventFlags;

    // Pointer array -- devices are heap-allocated on connect, freed on disconnect.
    USBDeviceTransport* _devices[MAX_USB_DEVICES];

    // Parallel array to track device handles per slot (needed for DEV_GONE lookup)
    usb_device_handle_t _handles[MAX_USB_DEVICES];

    TaskHandle_t _taskHandle;
    static void _usbTask(void* arg);

    static void _clientEventCallback(const usb_host_client_event_msg_t* msg, void* arg);
    void onNewDevice(uint8_t address);
    void onDeviceGone(usb_device_handle_t devHandle);

    int findFreeSlot() const;
    int findByHandle(usb_device_handle_t h) const;

    // Thread-safe deferred operations queue (USB task -> main loop)
    enum class PendingOp : uint8_t { ADD, REMOVE };
    struct PendingAction {
        PendingOp op;
        int slot;
    };
    QueueHandle_t _pendingQueue;  // FreeRTOS queue of PendingAction
    void processPendingOps();
};

#endif // USB_HUB_MANAGER_H
