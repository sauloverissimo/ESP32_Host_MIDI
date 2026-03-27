#include "USBHubManager.h"
#include "MIDIHandler.h"
#include <string.h>

static const char* TAG = "USBHubManager";

USBHubManager::USBHubManager()
  : _handler(nullptr),
    _clientHandle(nullptr),
    _eventFlags(0),
    _taskHandle(nullptr),
    _pendingQueue(nullptr)
{
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        _devices[i] = nullptr;
        _handles[i] = nullptr;
    }
}

USBHubManager::~USBHubManager() {
    // Stop the USB task
    if (_taskHandle) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }

    // Detach and delete all active devices
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        if (_devices[i]) {
            _devices[i]->detach();
            if (_handles[i]) {
                usb_host_device_close(_clientHandle, _handles[i]);
                _handles[i] = nullptr;
            }
            delete _devices[i];
            _devices[i] = nullptr;
        }
    }

    // Clean up the pending queue
    if (_pendingQueue) {
        vQueueDelete(_pendingQueue);
        _pendingQueue = nullptr;
    }

    // Deregister client
    if (_clientHandle) {
        usb_host_client_deregister(_clientHandle);
        _clientHandle = nullptr;
    }
}

bool USBHubManager::begin(MIDIHandler& handler) {
    _handler = &handler;

    // Install USB Host Library (may already be installed by USBConnection)
    usb_host_config_t config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };
    esp_err_t err = usb_host_install(&config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }
    // ESP_ERR_INVALID_STATE means already installed — that's fine.

    // Register as a USB Host client
    usb_host_client_config_t client_config = {
        .is_synchronous = true,
        .max_num_event_msg = 10,
        .async = {
            .client_event_callback = _clientEventCallback,
            .callback_arg = this
        }
    };
    err = usb_host_client_register(&client_config, &_clientHandle);
    if (err != ESP_OK) {
        return false;
    }

    // Create pending operations queue (depth 8)
    _pendingQueue = xQueueCreate(8, sizeof(PendingAction));
    if (!_pendingQueue) {
        usb_host_client_deregister(_clientHandle);
        _clientHandle = nullptr;
        return false;
    }

    // Create USB event handling task on core 0
    BaseType_t ret = xTaskCreatePinnedToCore(
        _usbTask, "usb_hub", 4096, this, 5, &_taskHandle, 0
    );
    if (ret != pdPASS) {
        vQueueDelete(_pendingQueue);
        _pendingQueue = nullptr;
        usb_host_client_deregister(_clientHandle);
        _clientHandle = nullptr;
        return false;
    }

    return true;
}

void USBHubManager::_usbTask(void* arg) {
    USBHubManager* mgr = static_cast<USBHubManager*>(arg);
    for (;;) {
        usb_host_lib_handle_events(1, &mgr->_eventFlags);
        usb_host_client_handle_events(mgr->_clientHandle, 1);
    }
}

void USBHubManager::_clientEventCallback(const usb_host_client_event_msg_t* msg, void* arg) {
    USBHubManager* mgr = static_cast<USBHubManager*>(arg);
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            mgr->onNewDevice(msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            mgr->onDeviceGone(msg->dev_gone.dev_hdl);
            break;
    }
}

void USBHubManager::onNewDevice(uint8_t address) {
    int slot = findFreeSlot();
    if (slot < 0) {
        return;  // No free slots
    }

    usb_device_handle_t devHandle = nullptr;
    esp_err_t err = usb_host_device_open(_clientHandle, address, &devHandle);
    if (err != ESP_OK) {
        return;
    }

    USBDeviceTransport* transport = new (std::nothrow) USBDeviceTransport();
    if (!transport) {
        usb_host_device_close(_clientHandle, devHandle);
        return;
    }

    if (!transport->attach(_clientHandle, devHandle, address)) {
        // No MIDI interface found or attach failed
        delete transport;
        usb_host_device_close(_clientHandle, devHandle);
        return;
    }

    // Store in slot
    _devices[slot] = transport;
    _handles[slot] = devHandle;

    // Enqueue ADD operation for main loop
    PendingAction action = { PendingOp::ADD, slot };
    xQueueSend(_pendingQueue, &action, 0);
}

void USBHubManager::onDeviceGone(usb_device_handle_t devHandle) {
    int slot = findByHandle(devHandle);
    if (slot < 0) {
        return;  // Unknown device
    }

    // Detach the transport (releases interface, frees transfer)
    if (_devices[slot]) {
        _devices[slot]->detach();
    }

    // Close the USB device handle
    usb_host_device_close(_clientHandle, devHandle);
    _handles[slot] = nullptr;

    // Enqueue REMOVE operation for main loop
    PendingAction action = { PendingOp::REMOVE, slot };
    xQueueSend(_pendingQueue, &action, 0);
}

void USBHubManager::task() {
    processPendingOps();
}

void USBHubManager::processPendingOps() {
    PendingAction action;
    while (xQueueReceive(_pendingQueue, &action, 0) == pdTRUE) {
        if (action.slot < 0 || action.slot >= MAX_USB_DEVICES) {
            continue;
        }
        switch (action.op) {
            case PendingOp::ADD:
                if (_handler && _devices[action.slot]) {
                    _handler->addTransport(_devices[action.slot]);
                }
                break;
            case PendingOp::REMOVE:
                if (_handler && _devices[action.slot]) {
                    _handler->removeTransport(_devices[action.slot]);
                }
                delete _devices[action.slot];
                _devices[action.slot] = nullptr;
                break;
        }
    }
}

int USBHubManager::connectedDevices() const {
    int count = 0;
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        if (_devices[i] != nullptr) {
            count++;
        }
    }
    return count;
}

USBDeviceTransport* USBHubManager::device(int index) {
    if (index < 0 || index >= MAX_USB_DEVICES) {
        return nullptr;
    }
    return _devices[index];
}

int USBHubManager::findFreeSlot() const {
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        if (_devices[i] == nullptr) {
            return i;
        }
    }
    return -1;
}

int USBHubManager::findByHandle(usb_device_handle_t h) const {
    for (int i = 0; i < MAX_USB_DEVICES; i++) {
        if (_handles[i] == h) {
            return i;
        }
    }
    return -1;
}
