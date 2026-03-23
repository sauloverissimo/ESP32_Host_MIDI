#ifndef MIDI_TRANSPORT_H
#define MIDI_TRANSPORT_H

#include <cstdint>
#include <cstddef>

// Abstract base class for MIDI transports.
// USB, BLE, ESP-NOW, RTP-MIDI — any transport implements this interface.
// MIDIHandler operates on MIDITransport pointers, decoupled from specifics.
//
// Usage for custom transports:
//   class MyTransport : public MIDITransport {
//       void task() override { /* read hardware, call dispatchMidiData() */ }
//       bool isConnected() const override { return connected; }
//   };
//   midiHandler.addTransport(&myTransport);

class MIDITransport {
public:
    typedef void (*MIDIDataCallback)(void* context, const uint8_t* data, size_t length);
    typedef void (*ConnectionCallback)(void* context);

    virtual ~MIDITransport() = default;

    // Core interface — every transport must implement:
    virtual void task() = 0;
    virtual bool isConnected() const = 0;

    // Human-readable name for identification (e.g. "USB Host", "BLE Server").
    virtual const char* name() const { return "Transport"; }

    // Optional: send MIDI bytes (not all transports support sending).
    // Returns true if the message was sent successfully.
    virtual bool sendMidiMessage(const uint8_t* data, size_t length) { return false; }

    // Callback registration — used by MIDIHandler to receive data and events.
    void setMidiCallback(MIDIDataCallback cb, void* ctx) {
        _midiCb = cb; _midiCtx = ctx;
    }
    void setConnectionCallbacks(ConnectionCallback onConn, ConnectionCallback onDisconn, void* ctx) {
        _onConnect = onConn; _onDisconnect = onDisconn; _connCtx = ctx;
    }

    typedef void (*SysExDataCallback)(void* context, const uint8_t* data, size_t length);
    void setSysExCallback(SysExDataCallback cb, void* ctx) {
        _sysExCb = cb; _sysExCtx = ctx;
    }

    // UMP callback — delivers raw 32-bit UMP words (MIDI 2.0 native).
    // Only fired when transport negotiated MIDI 2.0 (Alt 1 / UMP endpoint).
    typedef void (*UMPDataCallback)(void* context, const uint32_t* words, uint8_t count);
    void setUMPCallback(UMPDataCallback cb, void* ctx) {
        _umpCb = cb; _umpCtx = ctx;
    }

protected:
    // Transport implementations call these to deliver data/events to the consumer.
    void dispatchMidiData(const uint8_t* data, size_t len) {
        if (_midiCb) _midiCb(_midiCtx, data, len);
    }
    void dispatchSysExData(const uint8_t* data, size_t len) {
        if (_sysExCb) _sysExCb(_sysExCtx, data, len);
    }
    void dispatchUMPData(const uint32_t* words, uint8_t count) {
        if (_umpCb) _umpCb(_umpCtx, words, count);
    }
    void dispatchConnected() { if (_onConnect) _onConnect(_connCtx); }
    void dispatchDisconnected() { if (_onDisconnect) _onDisconnect(_connCtx); }

private:
    MIDIDataCallback _midiCb = nullptr;
    void* _midiCtx = nullptr;
    SysExDataCallback _sysExCb = nullptr;
    void* _sysExCtx = nullptr;
    UMPDataCallback _umpCb = nullptr;
    void* _umpCtx = nullptr;
    ConnectionCallback _onConnect = nullptr;
    ConnectionCallback _onDisconnect = nullptr;
    void* _connCtx = nullptr;
};

#endif // MIDI_TRANSPORT_H
