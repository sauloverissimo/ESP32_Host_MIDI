#ifndef ESP32_PIN_CONFIG_H
#define ESP32_PIN_CONFIG_H

// Configurações para comunicação USB
#define USB_DP_PIN   19
#define USB_DN_PIN   18

// Configurações para o display T-Display S3
#define TFT_CS_PIN    6
#define TFT_DC_PIN    16
#define TFT_RST_PIN   5
#define TFT_BL_PIN    38

// Configurações para o áudio via I2S (utilizado pelo PCM5102A no exemplo)
#define I2S_BCK_PIN        11   // Conectado ao BCK do PCM5102A
#define I2S_WS_PIN         13   // Conectado ao LRCK do PCM5102A
#define I2S_DATA_OUT_PIN   12   // Conectado ao DIN do PCM5102A

#define PIN_POWER_ON 15

#define PIN_BUTTON_1 0
#define PIN_BUTTON_2 14
#define PIN_BAT_VOLT 4

#define PIN_IIC_SCL 17
#define PIN_IIC_SDA 18

#endif // ESP32_PIN_CONFIG_H
