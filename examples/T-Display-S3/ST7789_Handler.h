#ifndef ST7789_HANDLER_H
#define ST7789_HANDLER_H

#include <LovyanGFX.h>
#include <vector>
#include <string>

// Classe ST7789_Handler:
// Gerencia o display ST7789 do T‑Display-S3 (baseado no código previamente implementado em DisplayHandler).
class ST7789_Handler {
public:
  ST7789_Handler();

  // Inicializa o display (configuração de rotação, fonte, cor, etc.)
  void init();

  // Exibe uma string formatada na tela, evitando flickering.
  void print(const std::string& message);

  // Exibe um vetor de strings concatenado na tela, evitando flickering.
  void print(const std::vector<std::string>& messages);

  // Exibe uma string formatada na tela que pula linha, evitando flickering.
  void println(const std::string& message);

  // Exibe um vetor de strings concatenado na tela que pula linha, evitando flickering.
  void println(const std::vector<std::string>& messages);


  // Limpa o display
  void clear();

private:
  // Classe customizada baseada em LovyanGFX para configurar o painel ST7789 via bus paralelo de 8 bits
  class LGFX : public lgfx::LGFX_Device {
  public:
    LGFX(void) {
      {  // Configuração do bus paralelo de 8 bits
        auto cfg = _bus_instance.config();
        cfg.pin_wr = 8;
        cfg.pin_rd = 9;
        cfg.pin_rs = 7;  // D/C
        cfg.pin_d0 = 39;
        cfg.pin_d1 = 40;
        cfg.pin_d2 = 41;
        cfg.pin_d3 = 42;
        cfg.pin_d4 = 45;
        cfg.pin_d5 = 46;
        cfg.pin_d6 = 47;
        cfg.pin_d7 = 48;
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
      }
      {  // Configuração do painel ST7789
        auto cfg = _panel_instance.config();
        cfg.pin_cs = 6;
        cfg.pin_rst = 5;
        cfg.pin_busy = -1;
        cfg.offset_rotation = 1;
        cfg.offset_x = 35;
        cfg.readable = false;
        cfg.invert = true;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;
        // Para orientação horizontal: display de 170 x 320
        cfg.panel_width = 170;
        cfg.panel_height = 320;
        _panel_instance.config(cfg);
      }
      setPanel(&_panel_instance);
      {  // Configuração da luz de fundo
        auto cfg = _light_instance.config();
        cfg.pin_bl = 38;
        cfg.invert = false;
        cfg.freq = 22000;
        cfg.pwm_channel = 7;
        _light_instance.config(cfg);
        _panel_instance.setLight(&_light_instance);
      }
    }
  private:
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Light_PWM _light_instance;
  };

  LGFX tft;
};

extern ST7789_Handler display;

#endif  // ST7789_HANDLER_H
