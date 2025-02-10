#ifndef ESP32_HOST_MIDI_H
#define ESP32_HOST_MIDI_H

#include <Arduino.h>
#include <usb/usb_host.h>

/*
  Biblioteca para comunicação USB MIDI via ESP32.
  O método onMidiMessage é virtual e deverá ser sobrescrito para tratar a mensagem.
*/

class ESP32_Host_MIDI {
public:
  bool isReady;            // Indica se a transferência USB está configurada
  uint8_t interval;        // Intervalo (bInterval do endpoint)
  unsigned long lastCheck; // Controle de temporização
  
  usb_host_client_handle_t clientHandle;
  usb_device_handle_t deviceHandle;
  uint32_t eventFlags;
  usb_transfer_t *midiTransfer; // Ponteiro para a transferência USB

  ESP32_Host_MIDI();

  // Inicializa o USB Host e registra o cliente
  void begin();

  // Deve ser chamado periodicamente para tratar eventos USB
  void task();

  // Método virtual para tratar as mensagens MIDI (dados brutos, incluindo cabeçalho)
  virtual void onMidiMessage(const uint8_t *data, size_t length);

protected:
  static void _clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg);
  static void _onReceive(usb_transfer_t *transfer);
  void _processConfig(const usb_config_desc_t *config_desc);
};

#endif // ESP32_HOST_MIDI_H
