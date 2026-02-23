#ifndef MAPPING_H
#define MAPPING_H

// Pin mapping for T-Display S3

// USB Host
#define USB_DP_PIN   19
#define USB_DN_PIN   18

// ST7789 Display
#define TFT_CS_PIN    6
#define TFT_DC_PIN    16
#define TFT_RST_PIN   5
#define TFT_BL_PIN    38

// Board hardware
#define PIN_POWER_ON 15
#define PIN_BUTTON_1  0
#define PIN_BUTTON_2 14

// WiFi credentials
#define WIFI_SSID  "YourSSID"
#define WIFI_PASS  "YourPassword"

// OSC ports
#define OSC_LOCAL_PORT   8000
#define OSC_REMOTE_PORT  9000

// IP of the computer running Max/MSP, Pure Data, SuperCollider, etc.
// Set to IPAddress(0,0,0,0) to disable sending (receive-only mode).
#define OSC_TARGET_IP  IPAddress(192, 168, 1, 100)

#endif // MAPPING_H
