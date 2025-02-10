#include "displayhandler.h"

// Define TFT_GRAY se não estiver definido
#ifndef TFT_GRAY
#define TFT_GRAY 0x8410
#endif

DisplayHandler::DisplayHandler() {
  // Construtor simples
}

void DisplayHandler::init() {
  tft.init();
  tft.setRotation(2);   // Rotaciona 180° (inverte o display)
  tft.setTextSize(1);   // Fonte pequena para as linhas regulares
  tft.setTextColor(TFT_WHITE);
  tft.fillScreen(TFT_BLACK);
}

void DisplayHandler::printMidiMessage(const char* message, float frequency, int amplitude) {
  tft.fillScreen(TFT_BLACK);
  
  String msg(message);
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
  
  int x = 2;
  int y = 10;

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(line1, x, y); y += 12;
  tft.drawString(line2, x, y); y += 12;
  tft.setTextSize(2);
  tft.drawString(line3, x, y); y += 16;
  tft.setTextSize(2);
  tft.drawString(line4, x, y); y += 16;

  int lineWidth = 320 - 2 * x;
  tft.drawFastHLine(x, y, lineWidth, TFT_GRAY);
  y += 2;

  tft.setTextSize(4);
  tft.setTextColor(TFT_RED);
  tft.drawString(line5, x, y);
  y += 30;

  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Freq: " + String(frequency, 2) + " Hz", x, y);
  y += 16;
  tft.drawString("Amp: " + String(amplitude), x, y);
}


void DisplayHandler::clear() {
  tft.fillScreen(TFT_BLACK);
}
