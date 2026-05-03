#include "BLEConnection.h"

// CCCD descriptor handling.
//
// The BLE library shipped with arduino-esp32 is Neil Kolban / Dariusz Krempa
// BluedroidArduino on BOTH v2.x and v3.x as of v3.1.x — the migration to
// NimBLE-Arduino is planned for a later v3.x release but has not landed in
// arduino-esp32 v3.1.3 (the current pin under pioarduino@53.03.13). The
// previous v6.0.0 gate `ESP_ARDUINO_VERSION_MAJOR >= 3` confused the Arduino
// core version with the BLE stack identity and silently dropped BLE2902 on
// v3.x users who are still on Bluedroid, breaking notification subscribe
// from iOS / macOS / DAWs.
//
// Always include BLE2902. In Bluedroid the explicit CCCD is mandatory for
// clients to subscribe to notifications. In NimBLE-Arduino addDescriptor(
// BLE2902) is flagged [[deprecated]] but remains a working compatibility
// no-op; the build stays green and the behaviour is correct on either stack.
#include <BLE2902.h>

BLEConnection::BLEConnection()
    : pServer(nullptr), pCharacteristic(nullptr),
      pBleCallback(nullptr), pServerCallback(nullptr),
      sendMutex(nullptr),
      queueHead(0), queueTail(0),
      queueMux(portMUX_INITIALIZER_UNLOCKED)
{
}

BLEConnection::~BLEConnection() {
    if (sendMutex) {
        vSemaphoreDelete(sendMutex);
        sendMutex = nullptr;
    }
    delete pBleCallback;
    pBleCallback = nullptr;
    delete pServerCallback;
    pServerCallback = nullptr;
}

void BLEConnection::begin(const std::string& deviceName) {
    if (pServer) return;  // already initialized

    sendMutex = xSemaphoreCreateMutex();

    // BLEDevice::init signature differs by stack:
    //   v2.x BluedroidArduino : init(std::string)
    //   v3.x NimBLE-Arduino   : init(String)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    BLEDevice::init(String(deviceName.c_str()));
#else
    BLEDevice::init(deviceName);
#endif
    pServer = BLEDevice::createServer();

    // Server callbacks: handle connect/disconnect and restart advertising automatically.
    class ServerCallback : public BLEServerCallbacks {
    public:
        BLEConnection* bleCon;
        ServerCallback(BLEConnection* con) : bleCon(con) {}
        void onConnect(BLEServer*) override {
            bleCon->dispatchConnected();
        }
        void onDisconnect(BLEServer*) override {
            // Flush pending data from the disconnected central.
            portENTER_CRITICAL(&bleCon->queueMux);
            bleCon->queueHead = 0;
            bleCon->queueTail = 0;
            portEXIT_CRITICAL(&bleCon->queueMux);

            bleCon->dispatchDisconnected();
            // Restart advertising so a new central can connect.
            BLEDevice::startAdvertising();
        }
    };
    delete pServerCallback;
    pServerCallback = new ServerCallback(this);
    pServer->setCallbacks(pServerCallback);

    BLEService* pService = pServer->createService(BLE_MIDI_SERVICE_UUID);

    // BLE MIDI spec requires READ + NOTIFY + WRITE_NR.
    pCharacteristic = pService->createCharacteristic(
        BLE_MIDI_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ    |
        BLECharacteristic::PROPERTY_NOTIFY  |
        BLECharacteristic::PROPERTY_WRITE_NR
    );

    // CCCD (0x2902) descriptor: explicit on every stack we currently ship.
    // BluedroidArduino requires it (no auto-create, no exposure of the
    // implicit CCCD via the Arduino wrapper); NimBLE-Arduino accepts it as
    // a deprecated compatibility shim. See file-top comment for why the
    // previous ESP_ARDUINO_VERSION_MAJOR gate was incorrect.
    pCharacteristic->addDescriptor(new BLE2902());

    // Receive callback: strips the 2-byte BLE MIDI header and enqueues raw MIDI bytes.
    // BLE MIDI packet format: [header][timestamp][midi_bytes...]
    // Processing is deferred to task() — same pattern as USBConnection.
    class BLECallback : public BLECharacteristicCallbacks {
    public:
        BLEConnection* bleCon;
        BLECallback(BLEConnection* con) : bleCon(con) {}
        void onWrite(BLECharacteristic* characteristic) override {
            // getValue() return type differs by stack:
            //   v2.x BluedroidArduino : returns std::string
            //   v3.x NimBLE-Arduino   : returns Arduino String
            // Both expose .length() and .c_str() with the signatures we use.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
            String rxValue = characteristic->getValue();
#else
            std::string rxValue = characteristic->getValue();
#endif
            size_t len = rxValue.length();
            // Minimum valid BLE MIDI packet: header + timestamp + 1 MIDI byte = 3 bytes
            if (len >= 3) {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(rxValue.c_str());
                // Skip header (byte 0) and timestamp (byte 1); enqueue raw MIDI bytes.
                // Processing happens in task() on the main loop — thread-safe.
                bleCon->enqueueMidiMessage(data + 2, len - 2);
            }
        }
    };
    delete pBleCallback;
    pBleCallback = new BLECallback(this);
    pCharacteristic->setCallbacks(pBleCallback);
    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_MIDI_SERVICE_UUID);
    // Enable scan response so the 128-bit BLE-MIDI service UUID stays in
    // the primary advertisement payload while the device name moves to the
    // SCAN_RSP. With setScanResponse(false) on arduino-esp32 v3.x the
    // primary payload fills up with name + auto-included TX power +
    // Peripheral Connection Interval AD types and Bluedroid silently drops
    // the 128-bit UUID. iOS BLE-MIDI apps (GarageBand, midimittr, MIDI
    // Wrench, Apollo MIDI, etc.) filter scan results by the BLE-MIDI
    // service UUID, so when it is missing the device is invisible to them.
    // arduino-esp32 v2.x did not auto-include the extra AD types and the
    // UUID happened to fit, masking this latent bug.
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

void BLEConnection::task() {
    // Drain the ring buffer and dispatch via MIDITransport callbacks.
    processQueue();
}

bool BLEConnection::isConnected() const {
    if (pServer)
        return (pServer->getConnectedCount() > 0);
    return false;
}

// ---------- Ring Buffer ----------
// Same pattern as USBConnection. Spinlock protects cross-task access:
// enqueue() runs in the BLE stack task; dequeue() runs in the main loop.

bool BLEConnection::enqueueMidiMessage(const uint8_t* data, size_t length) {
    portENTER_CRITICAL(&queueMux);
    int next = (queueHead + 1) % QUEUE_SIZE;
    if (next == queueTail) {
        // Queue full — discard to avoid blocking the BLE task.
        portEXIT_CRITICAL(&queueMux);
        return false;
    }
    size_t copyLen = (length > sizeof(bleQueue[0].data)) ? sizeof(bleQueue[0].data) : length;
    memcpy(bleQueue[queueHead].data, data, copyLen);
    bleQueue[queueHead].length = copyLen;
    queueHead = next;
    portEXIT_CRITICAL(&queueMux);
    return true;
}

bool BLEConnection::dequeueMidiMessage(RawBleMessage& msg) {
    portENTER_CRITICAL(&queueMux);
    if (queueTail == queueHead) {
        portEXIT_CRITICAL(&queueMux);
        return false;
    }
    msg = bleQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    portEXIT_CRITICAL(&queueMux);
    return true;
}

void BLEConnection::processQueue() {
    RawBleMessage msg;
    while (dequeueMidiMessage(msg)) {
        dispatchMidiData(msg.data, msg.length);
    }
}

// ---------- BLE MIDI Output ----------

bool BLEConnection::sendMidiMessage(const uint8_t* data, size_t length) {
    if (!pCharacteristic || !sendMutex || length == 0) return false;

    // Build BLE MIDI 1.0 packet before acquiring the mutex.
    //   Byte 0: Header    = 0x80 | ((timestamp_ms >> 7) & 0x3F)
    //   Byte 1: Timestamp = 0x80 | (timestamp_ms & 0x7F)
    //   Byte 2+: MIDI bytes (status + data bytes)
    uint32_t ts = (uint32_t)millis();
    uint8_t header    = 0x80 | ((ts >> 7) & 0x3F);
    uint8_t timestamp = 0x80 | (ts & 0x7F);

    size_t midiLen = (length > 18) ? 18 : length;  // safe limit for default BLE MTU
    uint8_t packet[20];
    packet[0] = header;
    packet[1] = timestamp;
    memcpy(packet + 2, data, midiLen);

    // Check connection inside the mutex to minimize TOCTOU race window.
    if (xSemaphoreTake(sendMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;

    bool sent = false;
    if (isConnected()) {
        pCharacteristic->setValue(packet, 2 + midiLen);
        pCharacteristic->notify();
        sent = true;
    }

    xSemaphoreGive(sendMutex);
    return sent;
}
