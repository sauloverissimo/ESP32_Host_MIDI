#ifndef GINGO_FLOW_ADAPTER_H
#define GINGO_FLOW_ADAPTER_H

// Bridges ESP32_Host_MIDI transports into midi2flow, source-agnostically.
// Every MIDITransport exposes setUMPCallback(cb, ctx) and setMidiCallback(cb,
// ctx) -- both carry a ctx -- so we recover `this` from ctx and never need a
// singleton. MIDI 2.0 (UMP MT 0x4) is ingested verbatim; MIDI 1.0 bytes are
// turned into UMP MT 0x2 by Midi1ToUmp. The flow then drives FlowDisplayState.
//
// attach() each transport you use (e.g. a USBMIDI2Connection, which handles
// both MIDI 1.0 Alt 0 and MIDI 2.0 Alt 1, and a BLEConnection).

#include <Arduino.h>          // millis()
#include <MIDITransport.h>    // ESP32_Host_MIDI base transport
#include "Midi1ToUmp.h"
#include "FlowDisplayState.h"

class GingoFlowAdapter {
public:
    GingoFlowAdapter()
        : midi1_(0, [this](uint32_t w0, uint32_t w1) {
              uint32_t w[2] = {w0, w1};
              state_.ingest(w, 2, millis());
          }) {}

    // Register the flow taps on a transport. Call once per transport in use.
    void attach(MIDITransport& t) {
        t.setUMPCallback(&GingoFlowAdapter::onUMP, this);   // MIDI 2.0 (UMP)
        t.setMidiCallback(&GingoFlowAdapter::onMidi1, this); // MIDI 1.0 (bytes)
    }

    // Drain the flow and refresh the display-facing state. Call once per loop.
    void poll() { state_.poll(); }

    FlowDisplayState&       state()       { return state_; }
    const FlowDisplayState& state() const { return state_; }

private:
    static void onUMP(void* ctx, const uint32_t* words, uint8_t count) {
        static_cast<GingoFlowAdapter*>(ctx)->state_.ingest(words, count, millis());
    }
    static void onMidi1(void* ctx, const uint8_t* data, size_t len) {
        static_cast<GingoFlowAdapter*>(ctx)->midi1_.feed(data, len);
    }

    FlowDisplayState state_;
    Midi1ToUmp       midi1_;
};

#endif  // GINGO_FLOW_ADAPTER_H
