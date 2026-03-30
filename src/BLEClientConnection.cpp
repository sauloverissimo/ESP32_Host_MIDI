#include "BLEClientConnection.h"

// BLE MIDI Service UUID (same as BLEConnection server)
static const char* BLE_MIDI_SVC  = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
static const char* BLE_MIDI_CHAR = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

// Static instance registry
BLEClientConnection* BLEClientConnection::instances[MAX_INSTANCES] = { nullptr };
int BLEClientConnection::instanceCount = 0;

BLEClientConnection::BLEClientConnection() {}

BLEClientConnection::~BLEClientConnection() {
    if (pClient && _connected) {
        pClient->disconnect();
    }
    if (sendMutex) {
        vSemaphoreDelete(sendMutex);
    }
    delete clientCallback;
    delete scanCallback;
    delete foundAddress;
    unregisterInstance();
}

void BLEClientConnection::registerInstance() {
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (instances[i] == nullptr) {
            instances[i] = this;
            instanceCount++;
            return;
        }
    }
}

void BLEClientConnection::unregisterInstance() {
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (instances[i] == this) {
            instances[i] = nullptr;
            instanceCount--;
            return;
        }
    }
}

void BLEClientConnection::begin(const std::string& targetAddress, const std::string& deviceName) {
    targetAddr = targetAddress;
    targetDevName = deviceName;
    sendMutex = xSemaphoreCreateMutex();

    // Initialize BLE if not already done (safe to call multiple times)
    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("ESP32 MIDI Client");
    }

    pClient = BLEDevice::createClient();
    clientCallback = new ClientCB(this);
    pClient->setClientCallbacks(clientCallback);

    registerInstance();
    startScan();
}

void BLEClientConnection::startScan() {
    if (_scanning) return;
    BLEScan* pScan = BLEDevice::getScan();
    if (!scanCallback) {
        scanCallback = new ScanCB(this);
    }
    pScan->setAdvertisedDeviceCallbacks(scanCallback, false);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->start(5, false);  // 5 second scan, non-blocking
    _scanning = true;
}

void BLEClientConnection::task() {
    // If we found a device during scan, try to connect
    if (_shouldConnect && foundAddress && !_connected) {
        BLEAddress addr = *foundAddress;
        delete foundAddress;
        foundAddress = nullptr;
        _shouldConnect = false;
        _scanning = false;
        connectToDevice(addr);
    }

    // If disconnected and not scanning, restart scan after a brief delay
    if (!_connected && !_scanning && !_shouldConnect) {
        startScan();
    }

    // Drain ring buffer
    RawMsg msg;
    while (dequeue(msg)) {
        dispatchMidiData(msg.data, msg.length);
    }
}

bool BLEClientConnection::isConnected() const {
    return _connected;
}

bool BLEClientConnection::connectToDevice(BLEAddress addr) {
    if (!pClient->connect(addr)) {
        return false;
    }

    BLERemoteService* pService = pClient->getService(BLE_MIDI_SVC);
    if (!pService) {
        pClient->disconnect();
        return false;
    }

    pRemoteChar = pService->getCharacteristic(BLE_MIDI_CHAR);
    if (!pRemoteChar) {
        pClient->disconnect();
        return false;
    }

    // Register for notifications (incoming MIDI from peripheral)
    if (pRemoteChar->canNotify()) {
        pRemoteChar->registerForNotify(notifyCallbackStatic);
    }

    _connected = true;
    dispatchConnected();
    return true;
}

void BLEClientConnection::disconnect() {
    if (pClient && _connected) {
        pClient->disconnect();
    }
    _connected = false;
    pRemoteChar = nullptr;
}

std::string BLEClientConnection::getConnectedAddress() const {
    if (_connected && pClient) {
        String addr = pClient->getPeerAddress().toString();
        return std::string(addr.c_str());
    }
    return "";
}

// --- BLE Notifications (incoming MIDI from peripheral) ---

void BLEClientConnection::notifyCallbackStatic(BLERemoteCharacteristic* ch,
                                                uint8_t* data, size_t length, bool isNotify) {
    if (length < 3) return;

    // Find which instance owns this characteristic by matching the client's peer address
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (instances[i] && instances[i]->_connected && instances[i]->pRemoteChar == ch) {
            // Strip BLE MIDI header (2 bytes), enqueue raw MIDI
            instances[i]->enqueue(data + 2, length - 2);
            return;
        }
    }
}

// --- BLE MIDI Output ---

bool BLEClientConnection::sendMidiMessage(const uint8_t* data, size_t length) {
    if (!pRemoteChar || !sendMutex || length == 0 || !_connected) return false;

    // Build BLE MIDI packet: header + timestamp + MIDI bytes
    uint32_t ts = (uint32_t)millis();
    uint8_t header    = 0x80 | ((ts >> 7) & 0x3F);
    uint8_t timestamp = 0x80 | (ts & 0x7F);

    size_t midiLen = (length > 18) ? 18 : length;
    uint8_t packet[20];
    packet[0] = header;
    packet[1] = timestamp;
    memcpy(packet + 2, data, midiLen);

    if (xSemaphoreTake(sendMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;

    bool sent = false;
    if (_connected && pRemoteChar->canWrite()) {
        pRemoteChar->writeValue(packet, 2 + midiLen, false);
        sent = true;
    }

    xSemaphoreGive(sendMutex);
    return sent;
}

// --- Ring Buffer ---

bool BLEClientConnection::enqueue(const uint8_t* data, size_t length) {
    portENTER_CRITICAL(&queueMux);
    int next = (queueHead + 1) % QUEUE_SIZE;
    if (next == queueTail) {
        portEXIT_CRITICAL(&queueMux);
        return false;
    }
    size_t copyLen = (length > sizeof(rxQueue[0].data)) ? sizeof(rxQueue[0].data) : length;
    memcpy(rxQueue[queueHead].data, data, copyLen);
    rxQueue[queueHead].length = copyLen;
    queueHead = next;
    portEXIT_CRITICAL(&queueMux);
    return true;
}

bool BLEClientConnection::dequeue(RawMsg& msg) {
    portENTER_CRITICAL(&queueMux);
    if (queueTail == queueHead) {
        portEXIT_CRITICAL(&queueMux);
        return false;
    }
    msg = rxQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    portEXIT_CRITICAL(&queueMux);
    return true;
}

// --- Scan Callback ---

void BLEClientConnection::ScanCB::onResult(BLEAdvertisedDevice advertisedDevice) {
    bool hasMidiUUID = advertisedDevice.haveServiceUUID() &&
                       advertisedDevice.isAdvertisingService(BLEUUID(BLE_MIDI_SVC));
    bool hasTargetFilter = !parent->targetAddr.empty() || !parent->targetDevName.empty();

    // If no target filter is set, require MIDI UUID in the advertisement.
    // If a target filter is set, allow name/address match without MIDI UUID --
    // the MIDI service will be verified after connection in connectToDevice().
    if (!hasMidiUUID && !hasTargetFilter) {
        return;
    }

    // Filter by address if specified
    if (!parent->targetAddr.empty()) {
        String addr = advertisedDevice.getAddress().toString();
        if (std::string(addr.c_str()) != parent->targetAddr) return;
    }

    // Filter by name if specified
    if (!parent->targetDevName.empty()) {
        if (!advertisedDevice.haveName() ||
            std::string(advertisedDevice.getName().c_str()) != parent->targetDevName) return;
    }

    // Found a match: stop scan, store address for task() to connect on main loop
    BLEDevice::getScan()->stop();
    parent->_scanning = false;
    delete parent->foundAddress;
    parent->foundAddress = new BLEAddress(advertisedDevice.getAddress());
    parent->_shouldConnect = true;
}

// --- Client Callbacks ---

void BLEClientConnection::ClientCB::onConnect(BLEClient*) {
    // Connection state managed in connectToDevice()
}

void BLEClientConnection::ClientCB::onDisconnect(BLEClient*) {
    parent->_connected = false;
    parent->pRemoteChar = nullptr;
    // Flush ring buffer
    portENTER_CRITICAL(&parent->queueMux);
    parent->queueHead = 0;
    parent->queueTail = 0;
    portEXIT_CRITICAL(&parent->queueMux);
    parent->dispatchDisconnected();
}
