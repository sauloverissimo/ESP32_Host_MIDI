#ifndef ESP32_HOST_MIDI_H
#define ESP32_HOST_MIDI_H

#include <Arduino.h>
#include <usb/usb_host.h>
#include "ESP32_Host_MIDI_Config.h"  // Configuração de pinos e definições específicas do hardware

/*
  Biblioteca para comunicação USB MIDI via ESP32 (adaptada do EspUsbHost).
  Foca exclusivamente na recepção de mensagens MIDI via USB, descartando tratamentos para mouse/teclado.
*/

class ESP32_Host_MIDI {
public:
  bool isReady;            // Indica se a transferência USB já foi configurada
  uint8_t interval;        // Intervalo (bInterval do endpoint)
  unsigned long lastCheck; // Controle para temporização das submissões

  usb_host_client_handle_t clientHandle;
  usb_device_handle_t deviceHandle;
  uint32_t eventFlags;
  usb_transfer_t *midiTransfer; // Ponteiro para a transferência USB de dados MIDI

  ESP32_Host_MIDI();

  // Inicializa o USB host e registra o cliente
  void begin();

  // Deve ser chamado periodicamente (por exemplo, no loop()) para processar os eventos USB
  void task();

  // Callback virtual para tratar as mensagens MIDI recebidas.
  // data: buffer de dados; length: número de bytes válidos
  virtual void onMidiMessage(const uint8_t *data, size_t length);

private:
  // Callback estático para eventos do cliente USB
  static void _clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg);

  // Callback estático para transferência USB concluída
  static void _onReceive(usb_transfer_t *transfer);

  // Processa o descritor de configuração, procurando uma interface/endpoint de MIDI
  void _processConfig(const usb_config_desc_t *config_desc);
};

#endif // ESP32_HOST_MIDI_H
