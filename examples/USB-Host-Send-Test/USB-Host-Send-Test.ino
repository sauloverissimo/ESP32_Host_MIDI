// USB-Host-Send-Test — ESP32_Host_MIDI integration test
//
// Sends every type of standard MIDI message to a connected USB device,
// reporting pass/fail for each sendMidiMessage() call on Serial.
// Connect a USB MIDI device (keyboard, synth, interface) and open
// Serial Monitor at 115200. The test runs once after device detection.
//
// Tested message types:
//   Channel Voice: NoteOn, NoteOff, PolyPressure, CC, ProgramChange,
//                  ChannelPressure, PitchBend
//   System Common: MTC Quarter Frame, Song Position, Song Select,
//                  Tune Request
//   System Real-Time: Clock, Start, Continue, Stop, Active Sensing,
//                     System Reset
//
// Arduino IDE setup:
//   Tools > Board    -> ESP32-S3 (T-Display-S3 or DevKit)
//   Tools > USB Mode -> USB-OTG (TinyUSB)

#include <Arduino.h>
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>  // v6.0: transports are no longer auto-included

USBConnection usbHost;       // v6.0: explicit USB Host transport

static bool testDone = false;
static int passed = 0;
static int failed = 0;

// Helper: send raw bytes via the first USB transport and report result.
static bool sendRaw(const char* label, const uint8_t* data, size_t len) {
    bool ok = midiHandler.sendRaw(data, len);
    if (ok) {
        Serial.printf("  %-40s OK\n", label);
        passed++;
    } else {
        Serial.printf("  %-40s FAIL\n", label);
        failed++;
    }
    delay(5); // small gap between messages
    return ok;
}

// Helper: send via MIDIHandler convenience methods.
static bool sendConvenience(const char* label, bool result) {
    if (result) {
        Serial.printf("  %-40s OK\n", label);
        passed++;
    } else {
        Serial.printf("  %-40s FAIL\n", label);
        failed++;
    }
    delay(5);
    return result;
}

void runSendTests() {
    Serial.println("\n=== USB MIDI Send Integration Test ===\n");

    // --- Channel Voice (via convenience API) ---
    Serial.println("[Channel Voice - Convenience API]");
    sendConvenience("sendNoteOn  ch1 C4 vel100",
        midiHandler.sendNoteOn(1, 60, 100));
    sendConvenience("sendNoteOff ch1 C4",
        midiHandler.sendNoteOff(1, 60, 0));
    sendConvenience("sendControlChange ch1 CC64=127",
        midiHandler.sendControlChange(1, 64, 127));
    sendConvenience("sendControlChange ch1 CC64=0",
        midiHandler.sendControlChange(1, 64, 0));
    sendConvenience("sendProgramChange ch1 prog5",
        midiHandler.sendProgramChange(1, 5));
    sendConvenience("sendPitchBend ch1 center",
        midiHandler.sendPitchBend(1, 0));

    // --- Channel Voice (via raw bytes) ---
    Serial.println("\n[Channel Voice - Raw Bytes]");
    { uint8_t d[] = {0x90, 60, 80};  sendRaw("NoteOn  ch1 raw",  d, 3); }
    { uint8_t d[] = {0x80, 60, 0};   sendRaw("NoteOff ch1 raw",  d, 3); }
    { uint8_t d[] = {0xA0, 60, 50};  sendRaw("PolyPressure ch1", d, 3); }
    { uint8_t d[] = {0xB0, 1, 64};   sendRaw("CC ModWheel ch1",  d, 3); }
    { uint8_t d[] = {0xC0, 10};      sendRaw("ProgramChange ch1",d, 2); }
    { uint8_t d[] = {0xD0, 80};      sendRaw("ChanPressure ch1", d, 2); }
    { uint8_t d[] = {0xE0, 0, 0x40}; sendRaw("PitchBend center", d, 3); }

    // All 16 channels NoteOn
    Serial.println("\n[NoteOn all 16 channels]");
    for (uint8_t ch = 0; ch < 16; ch++) {
        char label[40];
        snprintf(label, sizeof(label), "NoteOn ch%d", ch + 1);
        uint8_t d[] = { (uint8_t)(0x90 | ch), 60, 100 };
        sendRaw(label, d, 3);
    }
    // NoteOff all 16 channels
    for (uint8_t ch = 0; ch < 16; ch++) {
        uint8_t d[] = { (uint8_t)(0x80 | ch), 60, 0 };
        sendRaw(ch == 0 ? "NoteOff ch1..16 (silent)" : "", d, 3);
    }

    // --- System Common ---
    Serial.println("\n[System Common]");
    { uint8_t d[] = {0xF1, 0x23};       sendRaw("MTC Quarter Frame",     d, 2); }
    { uint8_t d[] = {0xF2, 0x10, 0x20}; sendRaw("Song Position Pointer", d, 3); }
    { uint8_t d[] = {0xF3, 0x05};       sendRaw("Song Select",           d, 2); }
    { uint8_t d[] = {0xF6};             sendRaw("Tune Request",          d, 1); }

    // --- System Real-Time ---
    Serial.println("\n[System Real-Time]");
    { uint8_t d[] = {0xF8}; sendRaw("Timing Clock",   d, 1); }
    { uint8_t d[] = {0xFA}; sendRaw("Start",          d, 1); }
    { uint8_t d[] = {0xFB}; sendRaw("Continue",       d, 1); }
    { uint8_t d[] = {0xFC}; sendRaw("Stop",           d, 1); }
    { uint8_t d[] = {0xFE}; sendRaw("Active Sensing", d, 1); }
    // Note: 0xFF (System Reset) omitted by default - it resets some devices.
    // Uncomment to test:
    // { uint8_t d[] = {0xFF}; sendRaw("System Reset", d, 1); }

    // --- Edge cases ---
    Serial.println("\n[Edge Cases - Expected Failures]");
    { uint8_t d[] = {0xF4};  sendRaw("Undefined 0xF4 (expect FAIL)", d, 1); }
    { uint8_t d[] = {0x3C};  sendRaw("Data byte 0x3C (expect FAIL)", d, 1); }

    Serial.printf("\n=== Results: %d passed, %d failed ===\n\n", passed, failed);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("USB-Host-Send-Test: waiting for USB MIDI device...");

    midiHandler.addTransport(&usbHost);  // v6.0: explicit
    usbHost.begin();                      // v6.0: user owns lifecycle
    midiHandler.begin();
}

void loop() {
    midiHandler.task();

    // Run tests when the USB queue receives any data (device is alive).
    const auto &queue = midiHandler.getQueue();
    if (!testDone && !queue.empty()) {
        midiHandler.clearQueue();
        delay(200);
        runSendTests();
        testDone = true;
    }

    delay(10);
}
