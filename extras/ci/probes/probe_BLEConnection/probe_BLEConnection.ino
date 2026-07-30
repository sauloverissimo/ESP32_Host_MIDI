// CI probe — compiles <BLEConnection.h> in isolation.
//
// The Arduino IDE compiles every .cpp under src/, even for sketches that never
// include the matching header. A transport whose .cpp is not guarded by a
// feature macro therefore breaks the build on chips that lack the underlying
// hardware (see the ESP32-C6 / usb_host.h regression fixed in v7.1.0).
//
// One probe per transport, compiled across the full chip matrix, catches that
// class of regression before it reaches users.

#include <ESP32_Host_MIDI.h>
#include <BLEConnection.h>

void setup() {}
void loop() {}
