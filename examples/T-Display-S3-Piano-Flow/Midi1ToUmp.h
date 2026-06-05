#ifndef MIDI1_TO_UMP_H
#define MIDI1_TO_UMP_H

// Minimal MIDI 1.0 channel-voice bytestream -> UMP MT 0x2 converter, with
// running-status support. Realtime bytes (>= 0xF8) are ignored without breaking
// a message in progress; system common (0xF0..0xF7) is dropped (no SysEx in the
// MVP). Header-only, no allocation. Kept tiny and host-testable so the flow
// piano stays free of the AM_MIDI2.0Lib umpProcessor dependency.

#include <cstdint>
#include <cstddef>
#if !defined(__AVR__)
  #include <functional>
#endif

class Midi1ToUmp {
public:
    using Sink = std::function<void(uint32_t w0, uint32_t w1)>;
    Midi1ToUmp(uint8_t group, Sink sink) : group_(group), sink_(sink) {}

    void feed(const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) feedByte(data[i]);
    }

    void feedByte(uint8_t b) {
        if (b & 0x80) {                          // status byte
            if (b >= 0xF8) return;               // realtime: ignore, keep state
            if (b >= 0xF0) { status_ = 0; return; }  // system common: drop (MVP)
            status_ = b; haveD1_ = false; return; // channel-voice status (sets running status)
        }
        if (!status_) return;                    // data byte with no status
        if (!haveD1_) {
            d1_ = b; haveD1_ = true;
            if (needsTwo(status_)) return;       // wait for the second data byte
            emit(d1_, 0); haveD1_ = false;       // 1-data-byte message complete
            return;
        }
        emit(d1_, b); haveD1_ = false;           // 2-data-byte message complete (running status keeps status_)
    }

private:
    static bool needsTwo(uint8_t st) {
        uint8_t hi = st & 0xF0;
        return hi != 0xC0 && hi != 0xD0;         // program change / channel pressure = 1 data byte
    }

    void emit(uint8_t d1, uint8_t d2) {
        uint8_t status = status_ & 0xF0;
        uint8_t ch     = status_ & 0x0F;
        uint32_t w0 = ((uint32_t)0x2 << 28) | ((uint32_t)group_ << 24) |
                      ((uint32_t)(status >> 4) << 20) | ((uint32_t)ch << 16) |
                      ((uint32_t)d1 << 8) | (uint32_t)d2;
        sink_(w0, 0u);
    }

    uint8_t group_;
    uint8_t status_ = 0;
    uint8_t d1_     = 0;
    bool    haveD1_ = false;
    Sink    sink_;
};

#endif  // MIDI1_TO_UMP_H
