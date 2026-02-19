#ifndef ESP32_HOST_MIDI_H
#define ESP32_HOST_MIDI_H

// --- Feature detection macros ---
// These macros determine which transport layers are available at compile time
// based on the ESP32 target and SDK configuration.

// USB Host requires USB-OTG hardware (ESP32-S2 or ESP32-S3 only)
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
  #define ESP32_HOST_MIDI_HAS_USB 1
#else
  #define ESP32_HOST_MIDI_HAS_USB 0
#endif

// BLE requires Bluetooth support (ESP32, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2)
// ESP32-S2 does NOT have Bluetooth.
#if defined(CONFIG_BT_ENABLED)
  #define ESP32_HOST_MIDI_HAS_BLE 1
#else
  #define ESP32_HOST_MIDI_HAS_BLE 0
#endif

// PSRAM support (for history buffer)
#if defined(CONFIG_SPIRAM) || defined(CONFIG_SPIRAM_SUPPORT)
  #define ESP32_HOST_MIDI_HAS_PSRAM 1
#else
  #define ESP32_HOST_MIDI_HAS_PSRAM 0
#endif

// --- Conditional includes ---

#if ESP32_HOST_MIDI_HAS_USB
  #include "USBConnection.h"
#endif

#if ESP32_HOST_MIDI_HAS_BLE
  #include "BLEConnection.h"
#endif

#include "MIDIHandlerConfig.h"
#include "MIDIHandler.h"

#endif  // ESP32_HOST_MIDI_H
