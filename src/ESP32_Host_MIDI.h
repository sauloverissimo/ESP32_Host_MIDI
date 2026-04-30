#ifndef ESP32_HOST_MIDI_H
#define ESP32_HOST_MIDI_H

// --- Feature detection macros ---
// These macros determine which transport layers are available at compile time
// based on the ESP32 target and SDK configuration.

// USB Host requires USB-OTG hardware.
//   ESP32-S2 / ESP32-S3 : Full-Speed (12 Mbps)
//   ESP32-P4            : High-Speed (480 Mbps) — multiple devices via hub
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32P4)
  #define ESP32_HOST_MIDI_HAS_USB 1
#else
  #define ESP32_HOST_MIDI_HAS_USB 0
#endif

// BLE requires Bluetooth support (ESP32, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2).
// ESP32-S2 and ESP32-P4 do NOT have native Bluetooth.
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

// Native Ethernet MAC — ESP32-P4 only (requires an external PHY, e.g. LAN8720).
// Other chips can use a SPI Ethernet module (W5500) without this macro.
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  #define ESP32_HOST_MIDI_HAS_ETH_MAC 1
#else
  #define ESP32_HOST_MIDI_HAS_ETH_MAC 0
#endif

// --- Includes (v6.0) ---
//
// This umbrella header now exposes only the always-on pieces:
//   * MIDITransport          (the transport interface)
//   * MIDIHandlerConfig      (config struct used by MIDIHandler)
//   * MIDIHandler            (optional aggregator)
//
// In v5.x and earlier, this header auto-included USBConnection and
// BLEConnection so a single `#include <ESP32_Host_MIDI.h>` would arrange
// both into a default firmware. That made every consumer of the library
// pay for compiling those two files, even firmware that used neither.
// It also coupled the MIDIHandler API to those two transports
// specifically (the others required addTransport).
//
// In v6.0 the auto-includes are gone. Application code is expected to
// include each transport it actually uses, by name:
//
//   #include <USBConnection.h>          // USB Host MIDI 1.0
//   #include <USBMIDI2Connection.h>     // USB Host MIDI 2.0
//   #include <USBDeviceConnection.h>    // USB Device MIDI 1.0
//   #include <BLEConnection.h>
//   #include <UARTConnection.h>
//   #include <ESPNowConnection.h>
//   #include <RTPMIDIConnection.h>
//   #include <EthernetMIDIConnection.h>
//   #include <OSCConnection.h>
//   #include <MIDI2UDPConnection.h>
//
// MIDIHandler is still here, but stripped of its built-in transports.
// Use addTransport() to wire whichever transports you instantiate.
// See docs/migration-v6.md for v5 -> v6 migration.

#include "MIDITransport.h"
#include "MIDIHandlerConfig.h"
#include "MIDIHandler.h"

#endif  // ESP32_HOST_MIDI_H
