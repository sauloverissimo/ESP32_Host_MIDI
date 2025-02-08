#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <LovyanGFX.h>

/*
  Classe DisplayHandler:
  - Inicializa o display utilizando a biblioteca LovyanGFX.
  - Exibe mensagens MIDI formatadas em 5 linhas com fonte pequena, 
    com destaque para a última linha ("Octave").
*/
class DisplayHandler {
public:
  DisplayHandler();
  void init();
  void printMidiMessage(const char* message);
  void clear();

private:
  // Classe interna que estende a LGFX_Device da LovyanGFX e define a configuração do display
  class LGFX : public lgfx::LGFX_Device {
  public:
    LGFX();
  private:
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Light_PWM     _light_instance;
  };

  LGFX tft;
};

#endif // DISPLAYHANDLER_H
