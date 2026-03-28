# 🔧 SysEx (System Exclusive)

The `MIDIHandler` supports sending and receiving SysEx messages across all transports -- USB Host, USB Device, and UART (DIN-5). The implementation uses a separate queue from the normal event queue, with no breaking changes for existing code.

---

## What is SysEx

SysEx (System Exclusive) are variable-length MIDI messages delimited by `0xF0` (start) and `0xF7` (end). Unlike NoteOn, CC, or PitchBend which have a fixed size (2-3 bytes), a SysEx message can range from a few bytes to several kilobytes.

While channel messages (notes, CCs) are standardized and generic, SysEx carries **proprietary or universal** data that goes well beyond what conventional MIDI offers.

---

## What SysEx enables

**Device identification** -- send a Universal Identity Request (`F0 7E 7F 06 01 F7`) and receive back the manufacturer ID, family, model, and firmware version. Your ESP32 can auto-detect what is connected and adapt its behavior. Plugged in a Roland, a Korg, a Novation -- the ESP32 knows who it is talking to.

**Patch management** -- most synthesizers use SysEx for bulk dumps: patches, presets, wavetables, entire configurations. With the configurable buffer you can receive a complete dump, save it to SPIFFS/SD, and send it back later. Your ESP32 becomes a patch librarian.

**Deep parameter control** -- many devices expose parameters via SysEx that do not exist as CC. Filter types, oscillator shapes, effects routing -- things that go well beyond the 128 CC values. Some devices have hundreds of parameters accessible only via SysEx.

**Firmware updates** -- some MIDI devices accept firmware updates via SysEx. With `sendSysEx()` you can build an ESP32-based updater.

**MIDI Sample Dump** -- there is a standard (MMA Sample Dump Standard) for transferring audio samples via SysEx. It is slow, but it works, and some vintage samplers only support this method.

---

## How it works internally

### Reassembly at the transport level

Each transport handles reassembly differently:

**USB (Host and Device)** -- The USB MIDI 1.0 protocol fragments SysEx into 4-byte packets with a Code Index Number (CIN) in the header:

| CIN | Meaning | Data bytes |
|-----|---------|------------|
| `0x04` | SysEx starts or continues | 3 |
| `0x05` | SysEx ends with 1 byte | 1 |
| `0x06` | SysEx ends with 2 bytes | 2 |
| `0x07` | SysEx ends with 3 bytes | 3 |

The library accumulates CIN 0x04 packets in an internal buffer and, upon receiving 0x05/06/07, assembles the complete message and dispatches it via `dispatchSysExData()`.

**UART (DIN-5)** -- MIDI serial is simpler: bytes arrive sequentially. The parser accumulates everything between `0xF0` and `0xF7`. If a different status byte arrives in the middle (protocol error), the SysEx is aborted and the new status is processed normally.

### Separate queue

SysEx uses its own `std::deque<MIDISysExEvent>`, completely separate from the event queue (`getQueue()`). This was a deliberate design decision -- SysEx messages are variable-length and mixing them into the existing queue would break the API for anyone already using `getQueue()`.

---

## Configuration

```cpp
MIDIHandlerConfig config;
config.maxSysExSize   = 512;  // max bytes per message (includes F0 and F7)
config.maxSysExEvents = 8;    // how many messages to keep in the queue
midiHandler.begin(config);
```

| Parameter | Default | Description |
|-----------|---------|-------------|
| `maxSysExSize` | 512 | Maximum size of a message. Larger messages are truncated. 0 = disables SysEx. |
| `maxSysExEvents` | 8 | Queue capacity. Oldest messages are discarded. |

!!! tip "About buffer size"
    The ESP32-S3 has 512KB of SRAM. Even `maxSysExSize = 2048` works fine. The buffer is allocated on the heap (`std::vector<uint8_t>`), so consider available RAM if you are running BLE, WiFi, and display simultaneously.

---

## API

### Reception

```cpp
// Access the SysEx queue
const auto& queue = midiHandler.getSysExQueue();

for (const auto& msg : queue) {
    // msg.index     -- global counter (incrementing)
    // msg.timestamp -- millis() at the time of reception
    // msg.data      -- std::vector<uint8_t>, complete message (F0 ... F7)

    Serial.printf("SysEx #%d (%d bytes): ", msg.index, msg.data.size());
    for (uint8_t b : msg.data) Serial.printf("%02X ", b);
    Serial.println();
}

// Clear the queue
midiHandler.clearSysExQueue();
```

### Real-time callback

```cpp
// Optional: immediate callback instead of polling
midiHandler.setSysExCallback([](const uint8_t* data, size_t len) {
    // Called as soon as the complete message is assembled
    // data[0] = 0xF0, data[len-1] = 0xF7
});
```

### Sending

```cpp
// Send SysEx -- transmits to all transports
const uint8_t identityReq[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
bool ok = midiHandler.sendSysEx(identityReq, sizeof(identityReq));
// Returns false if the message does not start with F0 or does not end with F7
```

---

## Compatibility

The implementation follows the USB MIDI 1.0 spec (CIN-based reassembly) and standard MIDI serial framing. It works with any class-compliant device -- if it sends valid SysEx between `F0` and `F7`, the library captures it.

!!! warning "Error checking"
    SysEx has no native error checking in the MIDI spec. USB transport is reliable (CRC at the USB level), but UART/DIN-5 is raw serial. Some devices implement their own checksums within the SysEx payload -- the library delivers the raw bytes so you can validate at the application level.

---

## Full example

See the [`T-Display-S3-SysEx`](https://github.com/sauloverissimo/ESP32_Host_MIDI/tree/main/examples/T-Display-S3-SysEx) example -- SysEx monitor with Identity Request on button press, real-time display:

![T-Display-S3 SysEx Monitor](https://github.com/sauloverissimo/ESP32_Host_MIDI/raw/main/examples/T-Display-S3-SysEx/images/sysex.jpeg)
