#ifndef MIDI_TYPES_H
#define MIDI_TYPES_H

#include <cstdint>
#include <cstddef>

// Structure to store a raw USB packet.
// Although transfers can be up to 64 bytes, only the first 4 are relevant per USB-MIDI event.
struct RawUsbMessage {
    uint8_t data[64];
    size_t length;
};

#endif // MIDI_TYPES_H
