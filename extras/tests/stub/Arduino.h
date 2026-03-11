#pragma once
// Minimal Arduino.h stub for native (Linux/g++) compilation.
// Provides types and functions used by MIDIHandler and MIDI2Support.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

// Simulated millis() — returns a controllable fake timestamp.
// Tests can set g_fakeMillis to control time.
extern unsigned long g_fakeMillis;
inline unsigned long millis() { return g_fakeMillis; }

// Serial stub — swallow all output
struct FakeSerial {
    void begin(unsigned long) {}
    void print(const char*) {}
    void print(int) {}
    void println(const char* s = "") {}
    void println(int) {}
    int printf(const char*, ...) { return 0; }
};
extern FakeSerial Serial;
