#ifndef BLE_CLIENT_CONNECTION_H
#define BLE_CLIENT_CONNECTION_H

#include <Arduino.h>

// ESP32_HOST_MIDI_BLE_AVAILABLE is defined by BLEConnection.h; see the rationale
// there. Included first so this header is self-contained when the Arduino IDE
// compiles BLEClientConnection.cpp on its own.
#include "BLEConnection.h"

#if ESP32_HOST_MIDI_BLE_AVAILABLE

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
    // Returns false if the client registry is full (MAX_INSTANCES already in
    // use), in which case this client cannot receive notifications.
    bool begin(const std::string& targetAddress = "", const std::string& deviceName = "");

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
    bool _connected = false;
    volatile bool _shouldConnect = false;
    // Address of the matched peripheral, handed from the scan callback to
    // task() in textual form ("aa:bb:cc:dd:ee:ff"). Two constraints meet here:
    // the default BLEAddress constructor only exists from core 3.3.0 on, and
    // the byte order behind getNative() differs between cores, so a raw-byte
    // round-trip through the byte constructor can flip the address. toString()
    // and the String constructor are a stable pair on every supported core and
    // stack, and a fixed char buffer keeps the critical-section handoff free
    // of construction and allocation.
    static const uint8_t ADDR_STR_LEN = 18;  // "aa:bb:cc:dd:ee:ff" + NUL
    char foundAddr[ADDR_STR_LEN] = {0};
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 3, 0)
    uint8_t foundAddrType = 0;         // BLEAddress carries an address type from 3.3.0 on
#endif

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

    // A single shared scan callback drives all instances: the BLEScan singleton
    // exposes one advertised-device callback, so per-instance callbacks would
    // overwrite each other. onResult() dispatches each advertisement to every
    // registered instance's onAdvertisement().
    class ScanCB : public BLEAdvertisedDeviceCallbacks {
    public:
        void onResult(BLEAdvertisedDevice advertisedDevice) override;
    };

    // Applies this instance's address/name filter to an advertisement and, on a
    // match, stops the shared scan and hands the address to task() to connect.
    void onAdvertisement(BLEAdvertisedDevice& advertisedDevice);

    // Static notify callback adapter (BLE stack requirement)
    static void notifyCallbackStatic(BLERemoteCharacteristic* ch,
                                     uint8_t* data, size_t length, bool isNotify);

    // Scan-complete adapter: the BLEScan singleton invokes this when a scan
    // window ends. Clears the shared scanning flag so task() can open a fresh
    // window; without it, a peripheral powered on after the first window would
    // never be discovered.
    static void scanCompleteStatic(BLEScanResults results);

    // Instance registry for static callback routing (max 3 simultaneous clients)
    static const int MAX_INSTANCES = 3;
    static BLEClientConnection* instances[MAX_INSTANCES];
    bool registerInstance();
    void unregisterInstance();

    // One shared scan across all instances (the BLEScan singleton allows one).
    static bool s_scanning;
    static ScanCB* s_scanCallback;
};

#endif // ESP32_HOST_MIDI_BLE_AVAILABLE

#endif // BLE_CLIENT_CONNECTION_H
