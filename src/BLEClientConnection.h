#ifndef BLE_CLIENT_CONNECTION_H
#define BLE_CLIENT_CONNECTION_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <freertos/semphr.h>
#include "MIDITransport.h"

// BLE MIDI Client (Central role): scans for and connects to a BLE MIDI peripheral.
// Use this to receive MIDI from controllers or send MIDI to synths over BLE.
//
// Usage:
//   BLEClientConnection bleClient;
//   bleClient.begin();                    // start scanning
//   midiHandler.addTransport(&bleClient);
//
// The transport scans for devices advertising the BLE MIDI service UUID.
// It connects to the first matching device found (or a specific address/name if set).
//
// Multiple instances can coexist to connect to multiple peripherals.

class BLEClientConnection : public MIDITransport {
public:
    BLEClientConnection();
    virtual ~BLEClientConnection();

    // Begin scanning for BLE MIDI peripherals.
    // targetAddress: if non-empty, only connect to this specific device.
    // deviceName: optional filter by advertised name.
    void begin(const std::string& targetAddress = "", const std::string& deviceName = "");

    void task() override;
    bool isConnected() const override;
    bool sendMidiMessage(const uint8_t* data, size_t length) override;
    const char* name() const override { return _name; }

    // Set a custom name for this transport instance (e.g. "Foot Controller 1")
    void setName(const char* n) { _name = n; }

    // Get the address of the connected device (empty if not connected)
    std::string getConnectedAddress() const;

    // Disconnect from current peripheral and restart scanning
    void disconnect();

private:
    const char* _name = "BLE Client";

    BLEClient* pClient = nullptr;
    BLERemoteCharacteristic* pRemoteChar = nullptr;
    SemaphoreHandle_t sendMutex = nullptr;

    std::string targetAddr;
    std::string targetDevName;
    bool _scanning = false;
    bool _connected = false;
    bool _shouldConnect = false;
    BLEAddress* foundAddress = nullptr;

    // Ring buffer for incoming MIDI (same pattern as BLEConnection)
    static const int QUEUE_SIZE = 64;
    struct RawMsg {
        uint8_t data[20];
        size_t length;
    };
    RawMsg rxQueue[QUEUE_SIZE];
    volatile int queueHead = 0;
    volatile int queueTail = 0;
    portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

    bool enqueue(const uint8_t* data, size_t length);
    bool dequeue(RawMsg& msg);

    bool connectToDevice(BLEAddress addr);
    void startScan();

    // BLE callback classes
    class ClientCB : public BLEClientCallbacks {
    public:
        BLEClientConnection* parent;
        ClientCB(BLEClientConnection* p) : parent(p) {}
        void onConnect(BLEClient*) override;
        void onDisconnect(BLEClient*) override;
    };
    ClientCB* clientCallback = nullptr;

    class ScanCB : public BLEAdvertisedDeviceCallbacks {
    public:
        BLEClientConnection* parent;
        ScanCB(BLEClientConnection* p) : parent(p) {}
        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };
    ScanCB* scanCallback = nullptr;

    // Static notify callback adapter (BLE stack requirement)
    static void notifyCallbackStatic(BLERemoteCharacteristic* ch,
                                     uint8_t* data, size_t length, bool isNotify);

    // Instance registry for static callback routing (max 3 simultaneous clients)
    static const int MAX_INSTANCES = 3;
    static BLEClientConnection* instances[MAX_INSTANCES];
    static int instanceCount;
    void registerInstance();
    void unregisterInstance();
};

#endif // BLE_CLIENT_CONNECTION_H
