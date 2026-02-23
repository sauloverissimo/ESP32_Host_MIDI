// Ethernet-MIDI — hardware configuration
// Adjust these values for your specific setup.

// ---- W5500 SPI chip-select pin ----------------------------------------
// Default ESP32 VSPI: SCK=18, MISO=19, MOSI=23 — only CS needs to be set.
// Call SPI.begin(SCK, MISO, MOSI, CS) in setup() if using non-default pins.
#define ETH_CS_PIN  5

// ---- MAC address (must be unique on your LAN) -------------------------
// If you have multiple devices, change the last byte on each one.
static const uint8_t MY_MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// ---- IP configuration -------------------------------------------------
// 1 = DHCP (recommended — IP is printed on Serial after startup).
// 0 = static IP — fill STATIC_IP with the address you want.
#define USE_DHCP  1
static const IPAddress STATIC_IP(192, 168, 1, 200);
