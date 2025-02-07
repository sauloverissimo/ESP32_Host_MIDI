#ifndef ESP32_HOST_MIDI_CONFIG_H
#define ESP32_HOST_MIDI_CONFIG_H

/*
  Configurações de pinos para o ESP32_Host_MIDI.
  Baseado no T-Display S3 (ESP32-S3, Lilygo com display ST7789).
  Ajuste os valores conforme seu esquemático ou se for utilizar outro hardware.
*/

// Exemplos de pinos para a interface USB (caso o driver precise de definições explícitas)
#define USB_DP_PIN   19
#define USB_DN_PIN   18

// Definições para o display ST7789 (caso o projeto venha a interagir com o display)
#define TFT_CS_PIN    5
#define TFT_DC_PIN    16
#define TFT_RST_PIN   23
#define TFT_BL_PIN    4

// Outras definições podem ser adicionadas conforme necessário (botões, LEDs, etc).

#endif // ESP32_HOST_MIDI_CONFIG_H
