#include "ESP32_BLE.h"

ESP32_BLE::ESP32_BLE() : pServer(nullptr), pCharacteristic(nullptr), deviceConnected(false), midiCallback(nullptr) {}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    Serial.println("Central conectada.");
  }
  void onDisconnect(BLEServer* pServer) override {
    Serial.println("Central desconectada. Reiniciando advertisement.");
    BLEDevice::startAdvertising();
  }
};

void ESP32_BLE::begin() {
  BLEDevice::init("ESP32 MIDI BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(BLE_MIDI_SERVICE_UUID);
  // Configura a característica para receber mensagens via BLE (WRITE sem resposta)
  pCharacteristic = pService->createCharacteristic(
                      BLE_MIDI_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pCharacteristic->setCallbacks(new BLEMIDICharacteristicCallbacks(this));
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  // Inclui o UUID do serviço no pacote primário
  pAdvertising->setScanResponse(false);
  pAdvertising->addServiceUUID(BLE_MIDI_SERVICE_UUID);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("Advertisement iniciado.");
}

void ESP32_BLE::task() {
  // Tarefas periódicas, se necessário.
}

bool ESP32_BLE::isConnected() {
  if (pServer)
    return (pServer->getConnectedCount() > 0);
  return false;
}

void ESP32_BLE::setMidiMessageCallback(MIDIMessageCallback cb) {
  midiCallback = cb;
}

void ESP32_BLE::sendMidiMessage(const uint8_t* data, size_t length) {
  if (pCharacteristic != nullptr) {
    pCharacteristic->setValue(const_cast<uint8_t*>(data), length);
    pCharacteristic->notify();
  }
}

BLEMIDICharacteristicCallbacks::BLEMIDICharacteristicCallbacks(ESP32_BLE* ble) : esp32_ble(ble) {}

void BLEMIDICharacteristicCallbacks::onWrite(BLECharacteristic *pCharacteristic) {
  std::string rxValue = std::string(pCharacteristic->getValue().c_str());
  if (rxValue.length() > 0) {
    Serial.println("Mensagem BLE recebida:");
    Serial.println(rxValue.c_str());
    if (esp32_ble->midiCallback != nullptr) {
      const uint8_t* data = reinterpret_cast<const uint8_t*>(rxValue.data());
      size_t length = rxValue.length();
      esp32_ble->midiCallback(data, length);
    }
  }
}
