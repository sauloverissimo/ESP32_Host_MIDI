#ifndef USB_CONEXION_H
#define USB_CONEXION_H

#include <Arduino.h>
#include <usb/usb_host.h>
#include <freertos/portmacro.h>

/*
   Estrutura para armazenar um pacote USB bruto.
   Embora a transferência seja de 64 bytes, somente os 4 primeiros são relevantes.
*/
struct RawUsbMessage {
    uint8_t data[64];  // Buffer completo recebido (tamanho máximo 64 bytes)
    size_t length;     // Número real de bytes recebidos (será usado somente os 4 primeiros)
};

class USB_Conexion {
public:
    USB_Conexion();

    // Inicializa o USB Host e registra o cliente.
    void begin();

    // Deve ser chamado periodicamente para tratar os eventos USB.
    // Nesta versão, processQueue() é chamado para encaminhar os pacotes.
    void task();

    // Retorna se a conexão USB está pronta.
    bool isConnected() const { return isReady; }

    // Callback virtual para encaminhar os dados MIDI brutos (os 4 primeiros bytes).
    // A camada superior deve sobrescrever esse método para processar os dados.
    virtual void onMidiDataReceived(const uint8_t* data, size_t length);

    // Callbacks de conexão (vazios por padrão).
    virtual void onDeviceConnected();
    virtual void onDeviceDisconnected();

    // Métodos de acesso à fila (para depuração ou análise externa)
    int getQueueSize() const;
    const RawUsbMessage& getQueueMessage(int index) const;

protected:
    bool isReady;
    uint8_t interval;         // Intervalo de polling (em ms)
    unsigned long lastCheck;

    usb_host_client_handle_t clientHandle;
    usb_device_handle_t deviceHandle;
    uint32_t eventFlags;
    usb_transfer_t* midiTransfer; // Ponteiro para a transferência USB

    // Fila para armazenar os pacotes USB brutos.
    // Protegida por spinlock para acesso thread-safe no dual-core ESP32.
    static const int QUEUE_SIZE = 64;
    RawUsbMessage usbQueue[QUEUE_SIZE];
    volatile int queueHead;
    volatile int queueTail;
    portMUX_TYPE queueMux;

    // Dados de controle de conexão
    bool firstMidiReceived;
    bool isMidiDeviceConfirmed;
    String deviceName;

    // Funções auxiliares para gerenciar a fila.
    bool enqueueMidiMessage(const uint8_t* data, size_t length);
    bool dequeueMidiMessage(RawUsbMessage &msg);
    void processQueue();

    // Callbacks internos do USB Host.
    static void _clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg);
    static void _onReceive(usb_transfer_t *transfer);
    void _processConfig(const usb_config_desc_t *config_desc);
};

#endif // USB_CONEXION_H
