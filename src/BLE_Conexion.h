#ifndef BLE_CONEXION_H
#define BLE_CONEXION_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>

// UUIDs padrão para BLE MIDI – mantenha os mesmos definidos no seu projeto.
#define BLE_MIDI_SERVICE_UUID        "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define BLE_MIDI_CHARACTERISTIC_UUID "7772E5DB-3868-4112-A1A9-F2669D106BF3"

// Estrutura para armazenar um pacote BLE bruto (opcional, se for necessário para depuração).
struct RawBleMessage {
    uint8_t data[64];  // Buffer completo recebido (tamanho máximo 64 bytes)
    size_t length;     // Número real de bytes recebidos (será usado somente os 4 primeiros)
};

class BLE_Conexion {
public:
    // Tipo para callback de mensagem MIDI
    typedef void (*MIDIMessageCallback)(const uint8_t* data, size_t length);

    BLE_Conexion();

    // Inicializa o servidor BLE MIDI e inicia a publicidade.
    void begin();

    // Processa eventos BLE (geralmente não é necessário um task periódico).
    void task();

    // Retorna se há algum dispositivo BLE conectado.
    bool isConnected() const;

    // Define um callback para processar as mensagens MIDI recebidas via BLE.
    void setMidiMessageCallback(MIDIMessageCallback cb);

    // Callback virtual que é chamado sempre que uma mensagem BLE MIDI (4 bytes) é recebida.
    // A camada superior (ou subclasse) pode sobrescrever este método.
    virtual void onMidiDataReceived(const uint8_t* data, size_t length);

protected:
    BLEServer* pServer;
    BLECharacteristic* pCharacteristic;
    MIDIMessageCallback midiCallback;
};

#endif // BLE_CONEXION_H
