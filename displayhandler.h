#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <LovyanGFX.h>

/*
  Classe DisplayHandler:
  - Inicializa o display usando LovyanGFX.
  - Exibe mensagens MIDI formatadas em 5 linhas com fonte pequena, com destaque para a última linha ("Octave").
*/

class DisplayHandler {
public:
  DisplayHandler();
  
  // Inicializa o display (orientação horizontal 180°, fonte pequena)
  void init();

  // Exibe a mensagem MIDI na tela dividida em 5 linhas.
  // A última linha ("Octave") será destacada com uma linha separadora, fonte maior e cor diferenciada.
  void printMidiMessage(const char* message);

  // Limpa o display.
  void clear();

private:
  // Classe customizada do display baseada na LovyanGFX
  class LGFX : public lgfx::LGFX_Device {
  public:
    LGFX(void) {
      { // Configuração do bus paralelo de 8 bits
        auto cfg = _bus_instance.config();
        cfg.pin_wr  = 8;
        cfg.pin_rd  = 9;
        cfg.pin_rs  = 7;  // D/C
        cfg.pin_d0  = 39;
        cfg.pin_d1  = 40;
        cfg.pin_d2  = 41;
        cfg.pin_d3  = 42;
        cfg.pin_d4  = 45;
        cfg.pin_d5  = 46;
        cfg.pin_d6  = 47;
        cfg.pin_d7  = 48;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }
      { // Configuração do painel ST7789
        auto cfg = _panel_instance.config();
        cfg.pin_cs   = 6;
        cfg.pin_rst  = 5;
        cfg.pin_busy = -1;
        cfg.offset_rotation = 1;
        cfg.offset_x = 35;
        cfg.readable = false;
        cfg.invert   = true;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;
        // Para orientação horizontal, o display é 320 x 170
        cfg.panel_width  = 320;  
        cfg.panel_height = 170;
        _panel_instance.config(cfg);
      }
      setPanel(&_panel_instance);
      { // Configuração da luz de fundo
        auto cfg = _light_instance.config();
        cfg.pin_bl      = 38;
        cfg.invert      = false;
        cfg.freq        = 22000;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
      }
    }
  private:
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Light_PWM     _light_instance;
  };

  LGFX tft;
};

#endif // DISPLAYHANDLER_H
