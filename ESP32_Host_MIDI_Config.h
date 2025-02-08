#ifndef ESP32_HOST_MIDI_CONFIG_H
#define ESP32_HOST_MIDI_CONFIG_H

// Configurações para comunicação USB
#define USB_DP_PIN   19
#define USB_DN_PIN   18

// Configurações para o display T-Display S3
#define TFT_CS_PIN    6
#define TFT_DC_PIN    16
#define TFT_RST_PIN   5
#define TFT_BL_PIN    38

// // Configurações para I2S e DAC PCM5102A (ajuste conforme sua shield)
// #define I2S_BCK_PIN        26   // Bit Clock
// #define I2S_WS_PIN         25   // Word Select (LRCK)
// #define I2S_DATA_OUT_PIN   22   // Data Out
#define PCM5102A_MUTE_PIN PCA95x5::Port::P17
#define I2S_BCK_PIN        13   // Conectado ao BCK do PCM5102A
#define I2S_WS_PIN         15   // Conectado ao LRCK do PCM5102A
#define I2S_DATA_OUT_PIN   14   // Conectado ao DIN do PCM5102A

#endif // ESP32_HOST_MIDI_CONFIG_H
