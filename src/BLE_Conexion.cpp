#include "BLE_Conexion.h"

BLE_Conexion::BLE_Conexion()
    : pServer(nullptr), pCharacteristic(nullptr), pBleCallback(nullptr), midiCallback(nullptr)
{
}

BLE_Conexion::~BLE_Conexion() {
    delete pBleCallback;
    pBleCallback = nullptr;
}

void BLE_Conexion::begin(const std::string& deviceName) {
    BLEDevice::init(deviceName);
    pServer = BLEDevice::createServer();

    BLEService* pService = pServer->createService(BLE_MIDI_SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        BLE_MIDI_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE_NR
    );

    // Cria um callback para onWrite que extrai os 4 primeiros bytes da mensagem e os encaminha.
    class BLECallback : public BLECharacteristicCallbacks {
    public:
        BLE_Conexion* bleCon;
        BLECallback(BLE_Conexion* con) : bleCon(con) {}
        void onWrite(BLECharacteristic* characteristic) override {
            std::string rxValue = std::string(characteristic->getValue().c_str());
            // Se houver ao menos 4 bytes, extrai os 4 primeiros.
            if(rxValue.size() >= 4) {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(rxValue.data());
                // Chama o callback definido pelo usuário (se houver).
                if(bleCon->midiCallback) {
                    bleCon->midiCallback(data, 4);
                }
                // Chama também o callback virtual para que uma subclasse possa sobrescrever.
                bleCon->onMidiDataReceived(data, 4);
            }
        }
    };

    // Armazena o ponteiro para liberar no destrutor
    delete pBleCallback;
    pBleCallback = new BLECallback(this);
    pCharacteristic->setCallbacks(pBleCallback);
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_MIDI_SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    BLEDevice::startAdvertising();
}

void BLE_Conexion::task() {
    // No BLE geralmente não é necessário processamento periódico.
}

bool BLE_Conexion::isConnected() const {
    if(pServer)
        return (pServer->getConnectedCount() > 0);
    return false;
}

void BLE_Conexion::setMidiMessageCallback(MIDIMessageCallback cb) {
    midiCallback = cb;
}

void BLE_Conexion::onMidiDataReceived(const uint8_t* data, size_t length) {
    // Implementação padrão: não faz nada.
    (void)data;
    (void)length;
}
