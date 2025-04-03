# ESP32_Host_MIDI 🎹📡

![image](https://github.com/user-attachments/assets/bba1c679-6c76-45b7-aa29-a3201a69b36a)

Project developed for the Arduino IDE.

This project provides a complete solution for receiving, interpreting, and displaying MIDI messages via USB and BLE on the ESP32 (especially ESP32-S3) using the T‑Display S3. The library is modular and can be easily adapted to other hardware by modifying the configuration file(s).

---

## Overview

The **ESP32_Host_MIDI** library allows the ESP32 to:
- Act as a USB host for MIDI devices (via the **USB_Conexion** module),
- Function as a BLE MIDI server (via the **BLE_Conexion** module),
- Process and interpret MIDI messages (using the **MIDI_Handler** module), and
- Display formatted MIDI data on a display (via the **ST7789_Handler** module).

The core header **ESP32_Host_MIDI.h** integrates the pin configuration, USB/BLE connectivity, and MIDI handling functionalities.

---

## File Structure

### Core Library Files (in the `src/` folder)
- **ESP32_Pin_Config.h**  
  Defines the pin configuration for USB communication, display control, and audio output.  
  - *Examples:* `USB_DP_PIN`, `USB_DN_PIN`, `TFT_CS_PIN`, `TFT_DC_PIN`, `TFT_RST_PIN`, `TFT_BL_PIN`.

- **USB_Conexion.h / USB_Conexion.cpp**  
  Implements the USB host functionality to receive MIDI data from connected MIDI devices.  
  - **Key Functions:**  
    - `begin()`: Initializes the USB host and registers the client.
    - `task()`: Handles USB events and processes incoming data.
    - `onMidiDataReceived()`: Virtual function (to be overridden) for processing received MIDI messages (first 4 bytes).

- **BLE_Conexion.h / BLE_Conexion.cpp**  
  Implements the BLE MIDI server, enabling the ESP32 to receive MIDI messages via Bluetooth Low Energy.  
  - **Key Functions:**  
    - `begin()`: Initializes the BLE server and starts advertising the MIDI service.
    - `task()`: Processes BLE events (if needed).
    - `setMidiMessageCallback()`: Registers a callback to handle incoming BLE MIDI messages.
    - `onMidiDataReceived()`: Virtual function (to be overridden) for processing BLE MIDI messages.

- **MIDI_Handler.h / MIDI_Handler.cpp**  
  Processes and interprets raw MIDI data (removing USB headers when necessary) and manages MIDI events:
  - **Features:**  
    - Handles MIDI events (NoteOn and NoteOff).
    - Converts MIDI note numbers into musical notes (e.g., "C4").
    - Maintains the state of active notes and an optional history buffer (using PSRAM).
    - Provides utility functions to retrieve formatted MIDI event data.
  - **Key Functions:**  
    - `begin()`: Initializes the MIDI handler and associated USB/BLE connections.
    - `task()`: Processes incoming USB and BLE MIDI events.
    - `handleMidiMessage(const uint8_t* data, size_t length)`: Interprets MIDI messages and categorizes them into NoteOn, NoteOff, etc.
    - `addEvent(const MIDIEventData& event)`: Stores MIDI events in an event queue.
    - `processQueue()`: Manages the event queue, ensuring it stays within the configured limits.
    - `enableHistory(int capacity)`: Enables a history buffer in PSRAM for MIDI event storage.
    - `setQueueLimit(int maxEvents)`: Defines the maximum number of events stored in the queue.
    - `getActiveNotesString()`: Returns a formatted string listing active MIDI notes.
    - `getBlock(int block, const std::deque<MIDIEventData>& queue, const std::vector<std::string>& fields)`: Retrieves specific blocks of MIDI events.
    - `flushActiveNotes(unsigned long currentTime)`: Clears active notes when necessary.
    - `clearActiveNotesNow()`: Immediately clears active notes.
    - `getAnswer(const std::vector<std::string>& fields, bool includeLabels)`: Returns MIDI event details based on requested fields.

- **ST7789_Handler.h / ST7789_Handler.cpp**  
  Manages the ST7789 display on the T‑Display S3 using the LovyanGFX library:
  - **Key Functions:**  
    - `init()`: Initializes the display (rotation, font, colors, etc.).
    - `print()` / `println()`: Displays text on the screen while minimizing flickering.
    - `clear()`: Clears the display.

- **ESP32_Host_MIDI.h**  
  The core header that includes the pin configuration, USB/BLE connectivity, and MIDI handling modules.

### Example Sketches (in the `examples/T-Display-S3/` folder)
- Contains a sketch demonstrating:
  - USB and BLE MIDI reception,
  - Processing of MIDI messages using **MIDI_Handler**, and
  - Display of formatted MIDI data on the T‑Display S3 using **ST7789_Handler**.
- The `examples/T-Display-S3/images/` folder includes images showing the project in action.

---

## Operation

1. **MIDI USB-OTG Reception:**  
   When a MIDI device is connected via USB, the **USB_Conexion** module captures the MIDI data and passes it to **MIDI_Handler** for processing.

2. **MIDI BLE Reception:**  
   The **BLE_Conexion** module enables the ESP32 to operate as a BLE MIDI server, receiving MIDI messages from paired Bluetooth devices.

3. **MIDI Message Processing:**  
   **MIDI_Handler** interprets incoming MIDI messages (handling NoteOn and NoteOff events), converts MIDI note numbers into musical notes, and optionally stores events in a history buffer.

4. **Display Output:**  
   The **ST7789_Handler** module handles the display of formatted MIDI information on the T‑Display S3, ensuring smooth text rendering without flickering.

---

## Customization

The library is designed to be modular:
- You can modify **ESP32_Pin_Config.h** to change the pin assignments according to your hardware.
- The modules **USB_Conexion**, **BLE_Conexion**, **MIDI_Handler**, and **ST7789_Handler** can be extended or replaced to suit specific application requirements.

---

## Getting Started

1. **Install Dependencies:**  
   - Arduino IDE.
   - LovyanGFX library for display management.
   - Required BLE libraries (included in the ESP32 core).

2. **Load the Example:**  
   Open the example sketch from `examples/T-Display-S3/` in the Arduino IDE, adjust the pin configuration if necessary, and upload it to your ESP32-S3 board.

3. **Connect a MIDI Device:**  
   Use a USB MIDI device or pair a BLE MIDI device to test MIDI message reception and display.

---

## Contributing

Contributions, bug reports, and suggestions are welcome!  
Feel free to open an issue or submit a pull request on GitHub.

---

## License

This project is released under the MIT License. See the [LICENSE](LICENSE) file for details.
