#include "displayhandler.h"

// Se TFT_GRAY não estiver definido, defina-o (em RGB565, 0x8410 é um tom de cinza)
#ifndef TFT_GRAY
#define TFT_GRAY 0x8410
#endif

// Implementação da classe DisplayHandler

DisplayHandler::DisplayHandler() {
  // Construtor simples
}

void DisplayHandler::init() {
  tft.init();
  // Rotaciona o display 180° para a orientação desejada
  tft.setRotation(2);
  tft.setTextSize(2);       // Fonte pequena para as linhas regulares
  tft.setTextColor(TFT_WHITE);
  tft.fillScreen(TFT_BLACK);
}

void DisplayHandler::printMidiMessage(const char* message) {
  tft.fillScreen(TFT_BLACK);
  
  String msg(message);
  // Divide a mensagem em 5 linhas (separadas por '\n')
  String line1, line2, line3, line4, line5;
  int idx1 = msg.indexOf('\n');
  if (idx1 >= 0) {
    line1 = msg.substring(0, idx1);
    int idx2 = msg.indexOf('\n', idx1 + 1);
    if (idx2 >= 0) {
      line2 = msg.substring(idx1 + 1, idx2);
      int idx3 = msg.indexOf('\n', idx2 + 1);
      if (idx3 >= 0) {
        line3 = msg.substring(idx2 + 1, idx3);
        int idx4 = msg.indexOf('\n', idx3 + 1);
        if (idx4 >= 0) {
          line4 = msg.substring(idx3 + 1, idx4);
          line5 = msg.substring(idx4 + 1);
        }
      }
    }
  }
  
  int x = 10; // margem à esquerda
  int y = 10; // posição vertical inicial
  
  // Imprime as 4 primeiras linhas com fonte pequena
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(line1, x, y);
  y += 12;
  tft.drawString(line2, x, y);
  y += 12;
  tft.drawString(line3, x, y);
  y += 12;
  tft.drawString(line4, x, y);
  y += 12;
  
  // Desenha uma linha horizontal separadora
  int lineWidth = 320 - 2 * x;
  tft.drawFastHLine(x, y, lineWidth, TFT_GRAY);
  y += 2;
  
  // Imprime a quinta linha ("Octave") com fonte maior e cor diferenciada (vermelho)
  tft.setTextSize(4);
  tft.setTextColor(TFT_RED);
  tft.drawString(line5, x, y);
}

void DisplayHandler::clear() {
  tft.fillScreen(TFT_BLACK);
}

// Implementação da classe interna LGFX
DisplayHandler::LGFX::LGFX() {
  // Configuração do bus paralelo de 8 bits
  auto bus_cfg = _bus_instance.config();
  bus_cfg.pin_wr  = 8;
  bus_cfg.pin_rd  = 9;
  bus_cfg.pin_rs  = 7;
  bus_cfg.pin_d0  = 39;
  bus_cfg.pin_d1  = 40;
  bus_cfg.pin_d2  = 41;
  bus_cfg.pin_d3  = 42;
  bus_cfg.pin_d4  = 45;
  bus_cfg.pin_d5  = 46;
  bus_cfg.pin_d6  = 47;
  bus_cfg.pin_d7  = 48;
  _bus_instance.config(bus_cfg);
  _panel_instance.setBus(&_bus_instance);
  
  // Configuração do painel ST7789
  auto panel_cfg = _panel_instance.config();
  panel_cfg.pin_cs   = 6;
  panel_cfg.pin_rst  = 5;
  panel_cfg.pin_busy = -1;
  panel_cfg.offset_rotation = 0; // Sem offset de rotação extra
  panel_cfg.offset_x = 0;        // Zera o offset horizontal
  panel_cfg.offset_y = 0;        // Zera o offset vertical
  panel_cfg.readable = false;
  panel_cfg.invert   = true;
  panel_cfg.rgb_order = false;
  panel_cfg.dlen_16bit = false;
  panel_cfg.bus_shared = false;
  panel_cfg.panel_width  = 320;
  panel_cfg.panel_height = 170;
  panel_cfg.memory_width = 320;   // Define a memória para mapear toda a área
  panel_cfg.memory_height = 170;  
  _panel_instance.config(panel_cfg);
  
  setPanel(&_panel_instance);
  
  // Configuração da luz de fundo
  auto light_cfg = _light_instance.config();
  light_cfg.pin_bl      = 38;
  light_cfg.invert      = false;
  light_cfg.freq        = 22000;
  light_cfg.pwm_channel = 7;
  _light_instance.config(light_cfg);
  _panel_instance.setLight(&_light_instance);
}
