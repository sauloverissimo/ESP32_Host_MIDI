#ifndef MIDI_HANDLER_CONFIG_H
#define MIDI_HANDLER_CONFIG_H

// Configuration structure for MIDIHandler behavior.
// Create an instance, modify the desired fields, and pass it to midiHandler.begin(config).
// All fields have sensible defaults — you only need to change what you want to calibrate.
//
// Example:
//   MIDIHandlerConfig config;
//   config.maxEvents = 30;
//   config.chordTimeWindow = 50;
//   midiHandler.begin(config);

struct MIDIHandlerConfig {
    // --- Event Queue ---

    // Maximum number of events kept in the active queue (SRAM).
    // Older events are discarded when this limit is reached.
    // Range: 1-100. Higher values use more RAM.
    int maxEvents = 20;

    // --- Chord Detection ---

    // Time window (ms) for grouping notes into the same chord.
    // If a new NoteOn arrives within this window after the last NoteOn,
    // it joins the current chord. If it arrives after this window,
    // a new chord starts (even if previous notes are still held).
    // Set to 0 to use the legacy behavior (new chord only when all notes released).
    // Typical values: 30-80ms for keyboard, 0 for pad controllers.
    unsigned long chordTimeWindow = 0;

    // --- Velocity Filter ---

    // Minimum velocity to accept a NoteOn event.
    // NoteOn messages with velocity below this threshold are ignored.
    // Useful for filtering ghost notes from sensitive keyboards.
    // Set to 0 to accept all velocities (default).
    // Range: 0-127.
    int velocityThreshold = 0;

    // --- History (PSRAM) ---

    // Initial capacity for the PSRAM history buffer.
    // Set to 0 to disable history (default).
    // Set to a positive value to enable history on begin().
    int historyCapacity = 0;

    // --- SysEx Configuration ---

    // Maximum size of a single SysEx message (bytes, including 0xF0 and 0xF7).
    // Messages larger than this are truncated. Set to 0 to disable SysEx.
    int maxSysExSize = 512;

    // Maximum number of SysEx messages in the queue.
    int maxSysExEvents = 8;

    // --- BLE Configuration ---

    // BLE device name used when advertising.
    // Only relevant when BLE is available on the hardware.
    const char* bleName = "ESP32 MIDI BLE";

    // Enable BLE transport at runtime. Set to false BEFORE calling begin()
    // to skip NimBLE initialization, freeing ~130 KB of RAM and eliminating
    // any BLE advertising.
    //
    // This replaces the earlier compile-time ESP32_HOST_MIDI_NO_BLE define,
    // which suffered from an ODR violation when set in a sketch (the library
    // .cpp was still compiled with BLE, causing silent memory corruption).
    // A runtime flag works reliably across translation units.
    bool enableBle = true;
};

#endif // MIDI_HANDLER_CONFIG_H
