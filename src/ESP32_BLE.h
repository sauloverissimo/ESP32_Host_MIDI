#ifndef ESP32_BLE_H
#define ESP32_BLE_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// UUIDs padrão para BLE MIDI
#define BLE_MIDI_SERVICE_UUID        "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define BLE_MIDI_CHARACTERISTIC_UUID "7772E5DB-3868-4112-A1A9-F2669D106BF3"

class ESP32_BLE {
public:
  // Definição do callback para mensagem MIDI BLE
  typedef void (*MIDIMessageCallback)(const uint8_t *data, size_t length);

  ESP32_BLE();

  // Inicializa o BLE MIDI Server e inicia a publicidade
  void begin();

  // Tarefa periódica (se necessário)
  void task();

  // Retorna se há algum dispositivo BLE conectado
  bool isConnected();

  // Define o callback para quando uma mensagem MIDI BLE é recebida
  void setMidiMessageCallback(MIDIMessageCallback cb);

  // Envia mensagem MIDI via BLE (opcional, se o servidor também precisar enviar)
  void sendMidiMessage(const uint8_t* data, size_t length);

  // Campos públicos para acesso na callback
  bool deviceConnected;
  MIDIMessageCallback midiCallback;

private:
  BLEServer* pServer;
  BLECharacteristic* pCharacteristic;
};

class BLEMIDICharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
  BLEMIDICharacteristicCallbacks(ESP32_BLE* ble);
  void onWrite(BLECharacteristic *pCharacteristic) override;
private:
  ESP32_BLE* esp32_ble;
};

#endif // ESP32_BLE_H
