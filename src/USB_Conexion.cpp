#include "USB_Conexion.h"
#include <string.h>

static bool isValidMidiMessage(const uint8_t* midiData, size_t length) {
    // Validação mínima: verifica se há pelo menos 2 bytes e o primeiro tem bit 7 set.
    if (length < 2) return false;
    if ((midiData[0] & 0x80) == 0) return false;
    uint8_t status = midiData[0] & 0xF0;
    if (status == 0xC0 || status == 0xD0)
        return (length >= 2);
    else
        return (length >= 3);
}

USB_Conexion::USB_Conexion()
  : isReady(false),
    interval(0),
    lastCheck(0),
    clientHandle(nullptr),
    deviceHandle(nullptr),
    eventFlags(0),
    midiTransfer(nullptr),
    queueHead(0),
    queueTail(0),
    queueMux(portMUX_INITIALIZER_UNLOCKED),
    firstMidiReceived(false),
    isMidiDeviceConfirmed(false),
    deviceName(""),
    lastError("")
{
}

bool USB_Conexion::begin() {
    usb_host_config_t config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&config);
    if (err != ESP_OK) {
        lastError = "USB host install failed (err=" + String(err) + ")";
        return false;
    }

    usb_host_client_config_t client_config = {
        .is_synchronous = true,
        .max_num_event_msg = 10,
        .async = {
            .client_event_callback = _clientEventCallback,
            .callback_arg = this,
        }
    };
    err = usb_host_client_register(&client_config, &clientHandle);
    if (err != ESP_OK) {
        lastError = "USB client register failed (err=" + String(err) + ")";
        return false;
    }
    lastError = "";
    return true;
}

void USB_Conexion::task() {
    usb_host_lib_handle_events(1, &eventFlags);
    usb_host_client_handle_events(clientHandle, 1);
    
    if (isReady && midiTransfer) {
        unsigned long now = millis();
        if ((now - lastCheck) > interval) {
            lastCheck = now;
            usb_host_transfer_submit(midiTransfer);
        }
    }
    // Chama processQueue() para encaminhar os pacotes para onMidiDataReceived().
    processQueue();
}

void USB_Conexion::onDeviceConnected() {
    // Implementação padrão (vazia). A camada superior pode sobrescrever.
}

void USB_Conexion::onDeviceDisconnected() {
    // Implementação padrão (vazia).
}

void USB_Conexion::onMidiDataReceived(const uint8_t* data, size_t length) {
    // Implementação padrão (vazia). A camada superior deve sobrescrever.
    (void)data;
    (void)length;
}

bool USB_Conexion::enqueueMidiMessage(const uint8_t* data, size_t /*length*/) {
    portENTER_CRITICAL(&queueMux);
    int next = (queueHead + 1) % QUEUE_SIZE;
    if (next == queueTail) {
        // Fila cheia; descarta a mensagem.
        portEXIT_CRITICAL(&queueMux);
        return false;
    }
    // Copia sempre os 4 primeiros bytes.
    size_t copyLength = 4;
    memcpy(usbQueue[queueHead].data, data, copyLength);
    usbQueue[queueHead].length = copyLength;
    queueHead = next;
    portEXIT_CRITICAL(&queueMux);
    return true;
}

bool USB_Conexion::dequeueMidiMessage(RawUsbMessage &msg) {
    portENTER_CRITICAL(&queueMux);
    if (queueTail == queueHead) {
        portEXIT_CRITICAL(&queueMux);
        return false;
    }
    msg = usbQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    portEXIT_CRITICAL(&queueMux);
    return true;
}

void USB_Conexion::processQueue() {
    RawUsbMessage msg;
    while (dequeueMidiMessage(msg)) {
        onMidiDataReceived(msg.data, msg.length);
    }
}

int USB_Conexion::getQueueSize() const {
    int size = queueHead - queueTail;
    if (size < 0) size += QUEUE_SIZE;
    return size;
}

const RawUsbMessage& USB_Conexion::getQueueMessage(int index) const {
    int realIndex = (queueTail + index) % QUEUE_SIZE;
    return usbQueue[realIndex];
}

// ---------- Callbacks Internos ----------

void USB_Conexion::_clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg) {
    USB_Conexion *usbCon = static_cast<USB_Conexion*>(arg);
    esp_err_t err;
    switch (eventMsg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            err = usb_host_device_open(usbCon->clientHandle, eventMsg->new_dev.address, &usbCon->deviceHandle);
            if (err != ESP_OK) {
                usbCon->lastError = "Device open failed (err=" + String(err) + ")";
                return;
            }
            {
                const usb_config_desc_t *config_desc;
                err = usb_host_get_active_config_descriptor(usbCon->deviceHandle, &config_desc);
                if (err != ESP_OK) {
                    usbCon->lastError = "Config descriptor failed (err=" + String(err) + ")";
                    return;
                }
                usbCon->_processConfig(config_desc);
            }
            usbCon->lastError = "";
            usbCon->onDeviceConnected();
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            if (usbCon->midiTransfer) {
                usb_host_transfer_free(usbCon->midiTransfer);
                usbCon->midiTransfer = nullptr;
            }
            usb_host_device_close(usbCon->clientHandle, usbCon->deviceHandle);
            usbCon->isReady = false;
            usbCon->onDeviceDisconnected();
            break;
        default:
            break;
    }
}

void USB_Conexion::_onReceive(usb_transfer_t *transfer) {
    USB_Conexion *usbCon = static_cast<USB_Conexion*>(transfer->context);
    if (transfer->status == 0 && transfer->actual_num_bytes > 0) {
        // Enfileira somente os 4 primeiros bytes.
        usbCon->enqueueMidiMessage(transfer->data_buffer, 4);
    }
    if (usbCon->isReady) {
        esp_err_t err = usb_host_transfer_submit(transfer);
        (void)err;
    }
}

void USB_Conexion::_processConfig(const usb_config_desc_t *config_desc) {
    const uint8_t* p = config_desc->val;
    uint16_t totalLength = config_desc->wTotalLength;
    uint16_t index = 0;
    bool claimedOk = false;

    while (index < totalLength) {
        // Validação de bounds: precisa de pelo menos 2 bytes para ler len e type
        if (index + 1 >= totalLength) break;
        uint8_t len = p[index];
        if (len < 2 || (index + len) > totalLength) break;  // Descriptor inválido ou além do limite

        uint8_t descriptorType = p[index + 1];
        if (descriptorType == 0x04) { // Interface Descriptor
            // Interface descriptor tem tamanho mínimo de 9 bytes
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
                esp_err_t err = usb_host_interface_claim(clientHandle, deviceHandle, bInterfaceNumber, bAlternateSetting);
                if (err == ESP_OK) {
                    uint16_t idx2 = index + len;
                    while (idx2 < totalLength) {
                        if (idx2 + 1 >= totalLength) break;
                        uint8_t len2 = p[idx2];
                        if (len2 < 2 || (idx2 + len2) > totalLength) break;
                        uint8_t type2 = p[idx2 + 1];
                        if (type2 == 0x04) break;  // Próxima interface
                        if (type2 == 0x05 && bNumEndpoints > 0) {
                            // Endpoint descriptor tem tamanho mínimo de 7 bytes
                            if (len2 >= 7) {
                                uint8_t bEndpointAddress = p[idx2 + 2];
                                uint8_t bmAttributes = p[idx2 + 3];
                                uint16_t wMaxPacketSize = (p[idx2 + 4] | (p[idx2 + 5] << 8));
                                uint8_t bInterval = p[idx2 + 6];
                                // Limita wMaxPacketSize para evitar alocações excessivas
                                if (wMaxPacketSize > 512) wMaxPacketSize = 512;
                                if (wMaxPacketSize == 0) wMaxPacketSize = 64;
                                if (bEndpointAddress & 0x80) {
                                    uint8_t transferType = bmAttributes & 0x03;
                                    uint32_t timeout = (transferType == 0x02) ? 3000 : 0;
                                    esp_err_t e2 = usb_host_transfer_alloc(wMaxPacketSize, timeout, &midiTransfer);
                                    if (e2 == ESP_OK && midiTransfer != nullptr) {
                                        midiTransfer->device_handle = deviceHandle;
                                        midiTransfer->bEndpointAddress = bEndpointAddress;
                                        midiTransfer->callback = _onReceive;
                                        midiTransfer->context = this;
                                        midiTransfer->num_bytes = wMaxPacketSize;
                                        interval = (bInterval == 0) ? 1 : bInterval;
                                        isReady = true;
                                        claimedOk = true;
                                        return;
                                    }
                                }
                            }
                        }
                        idx2 += len2;
                    }
                    usb_host_interface_release(clientHandle, deviceHandle, bInterfaceNumber);
                }
            }
        }
        index += len;
    }
    if (!claimedOk) {
        // Fallback: tenta um endpoint padrão com tamanho seguro
        esp_err_t err = usb_host_transfer_alloc(64, 3000, &midiTransfer);
        if (err == ESP_OK && midiTransfer != nullptr) {
            midiTransfer->device_handle = deviceHandle;
            midiTransfer->bEndpointAddress = 0x81;
            midiTransfer->callback = _onReceive;
            midiTransfer->context = this;
            midiTransfer->num_bytes = 64;
            interval = 1;
            isReady = true;
        }
    }
}
