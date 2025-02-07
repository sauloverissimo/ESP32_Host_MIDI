#include "ESP32_Host_MIDI.h"
#include <stdio.h>
#include <esp_log.h>

static const char* TAG = "ESP32_Host_MIDI";

ESP32_Host_MIDI::ESP32_Host_MIDI()
  : isReady(false),
    interval(0),
    lastCheck(0),
    clientHandle(nullptr),
    deviceHandle(nullptr),
    eventFlags(0),
    midiTransfer(nullptr)
{
}

void ESP32_Host_MIDI::begin() {
  const usb_host_config_t config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  esp_err_t err = usb_host_install(&config);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "usb_host_install() falhou: 0x%x", err);
  } else {
    ESP_LOGI(TAG, "usb_host_install() OK");
  }

  const usb_host_client_config_t client_config = {
    .is_synchronous = true,
    .max_num_event_msg = 10,
    .async = {
      .client_event_callback = _clientEventCallback,
      .callback_arg = this,
    }
  };

  err = usb_host_client_register(&client_config, &clientHandle);
  if (err != ESP_OK) {
    ESP_LOGI(TAG, "usb_host_client_register() falhou: 0x%x", err);
  } else {
    ESP_LOGI(TAG, "usb_host_client_register() OK");
  }
}

void ESP32_Host_MIDI::task() {
  esp_err_t err = usb_host_lib_handle_events(1, &eventFlags);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGI(TAG, "usb_host_lib_handle_events() erro: 0x%x", err);
  }
  err = usb_host_client_handle_events(clientHandle, 1);
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGI(TAG, "usb_host_client_handle_events() erro: 0x%x", err);
  }
  
  if (isReady && midiTransfer) {
    unsigned long now = millis();
    if ((now - lastCheck) > interval) {
      lastCheck = now;
      err = usb_host_transfer_submit(midiTransfer);
      if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "usb_host_transfer_submit() erro: 0x%x", err);
      }
    }
  }
}

void ESP32_Host_MIDI::_clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg) {
  ESP32_Host_MIDI *usbMidi = static_cast<ESP32_Host_MIDI*>(arg);
  esp_err_t err;
  switch (eventMsg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      ESP_LOGI(TAG, "Novo dispositivo detectado, endereço=%d", eventMsg->new_dev.address);
      err = usb_host_device_open(usbMidi->clientHandle, eventMsg->new_dev.address, &usbMidi->deviceHandle);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "usb_host_device_open() falhou: 0x%x", err);
        return;
      }
      const usb_config_desc_t *config_desc;
      err = usb_host_get_active_config_descriptor(usbMidi->deviceHandle, &config_desc);
      if (err != ESP_OK) {
        ESP_LOGI(TAG, "usb_host_get_active_config_descriptor() falhou: 0x%x", err);
        return;
      }
      usbMidi->_processConfig(config_desc);
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      ESP_LOGI(TAG, "Dispositivo desconectado");
      if (usbMidi->midiTransfer) {
        usb_host_transfer_free(usbMidi->midiTransfer);
        usbMidi->midiTransfer = nullptr;
      }
      usb_host_device_close(usbMidi->clientHandle, usbMidi->deviceHandle);
      usbMidi->isReady = false;
      break;
    default:
      ESP_LOGI(TAG, "Evento USB não tratado: %d", eventMsg->event);
      break;
  }
}

void ESP32_Host_MIDI::_processConfig(const usb_config_desc_t *config_desc) {
  const uint8_t *p = config_desc->val;
  uint16_t totalLength = config_desc->wTotalLength;
  uint16_t index = 0;
  uint8_t interfaceNumber = 0xFF;
  while (index < totalLength) {
    uint8_t length = p[index];
    uint8_t descriptorType = p[index + 1];
    if (length == 0) break;
    if (descriptorType == 0x04) { // Interface Descriptor
      uint8_t bInterfaceClass = p[index + 5];
      uint8_t bInterfaceSubClass = p[index + 6];
      if (bInterfaceClass == 0x01 && bInterfaceSubClass == 0x03) {
        interfaceNumber = p[index + 2];
        ESP_LOGI(TAG, "Interface MIDI Streaming encontrada: %d", interfaceNumber);
        esp_err_t err = usb_host_interface_claim(clientHandle, deviceHandle, interfaceNumber, p[index + 3]);
        if (err != ESP_OK) {
          ESP_LOGI(TAG, "Falha ao reivindicar a interface %d: 0x%x", interfaceNumber, err);
          return;
        }
      }
    }
    else if (descriptorType == 0x05 && interfaceNumber != 0xFF) { // Endpoint Descriptor
      uint8_t bEndpointAddress = p[index + 2];
      uint8_t bmAttributes = p[index + 3];
      uint16_t wMaxPacketSize = p[index + 4] | (p[index + 5] << 8);
      if (bEndpointAddress & 0x80) { // Endpoint IN
        uint8_t transferType = bmAttributes & 0x03;
        if (transferType == USB_BM_ATTRIBUTES_XFER_INT || transferType == USB_BM_ATTRIBUTES_XFER_BULK) {
          esp_err_t err = usb_host_transfer_alloc(wMaxPacketSize, 0, &midiTransfer);
          if (err != ESP_OK || midiTransfer == nullptr) {
            ESP_LOGI(TAG, "Falha ao alocar transferência USB: 0x%x", err);
            return;
          }
          midiTransfer->device_handle = deviceHandle;
          midiTransfer->bEndpointAddress = bEndpointAddress;
          midiTransfer->callback = _onReceive;
          midiTransfer->context = this;
          midiTransfer->num_bytes = wMaxPacketSize;
          interval = p[index + 6];
          isReady = true;
          ESP_LOGI(TAG, "Transferência MIDI alocada no endpoint 0x%x com tamanho %d", bEndpointAddress, wMaxPacketSize);
          break;
        }
      }
    }
    index += length;
  }
}

void ESP32_Host_MIDI::_onReceive(usb_transfer_t *transfer) {
  ESP32_Host_MIDI *usbMidi = static_cast<ESP32_Host_MIDI*>(transfer->context);
  if (transfer->status == 0 && transfer->actual_num_bytes > 0) {
    usbMidi->onMidiMessage(transfer->data_buffer, transfer->actual_num_bytes);
  } else {
    ESP_LOGI(TAG, "Erro na transferência USB ou dados vazios: status=%d, bytes=%d",
             transfer->status, transfer->actual_num_bytes);
  }
  if (usbMidi->isReady) {
    esp_err_t err = usb_host_transfer_submit(transfer);
    if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGI(TAG, "Falha ao reenviar a transferência USB: 0x%x", err);
    }
  }
}

void ESP32_Host_MIDI::onMidiMessage(const uint8_t *data, size_t length) {
  // Implementação padrão: imprime a mensagem no Serial Monitor.
  Serial.print("Mensagem MIDI (base): ");
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}
