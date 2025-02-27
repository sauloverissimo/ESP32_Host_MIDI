#include "ST7789_Handler.h"

// Define TFT_GRAY caso não esteja definido
#ifndef TFT_GRAY
#define TFT_GRAY 0x8410
#endif

ST7789_Handler::ST7789_Handler() {
  // Construtor simples (pode ser expandido se necessário)
}

void ST7789_Handler::init() {
  tft.init();
  tft.setRotation(2);   // Rotaciona 180° para correção da orientação
  tft.setTextSize(1);   // Fonte pequena para as linhas regulares
  tft.setTextColor(TFT_WHITE);
  tft.fillScreen(TFT_BLACK);
}

void ST7789_Handler::printMessage(const char* message, float /*frequency*/, int /*amplitude*/) {
    static String lastMessage = ""; // Armazena a última mensagem exibida

    // Se a mensagem não mudou, não redesenha para evitar flickering
    if (lastMessage == message) {
        return;
    }

    lastMessage = message; // Atualiza a mensagem armazenada

    tft.fillScreen(TFT_BLACK);  // Mantemos isso para apagar apenas quando a mensagem mudar
  
    // Ajustando para permitir mais linhas na tela
    const int totalLines = 12; // Permite exibir mais mensagens
    int margin = 2;
    int availableHeight = 240 - margin; 
    int lineHeight = availableHeight / totalLines; 

    int x = 2;
    int y = margin / 2;
  
    // Divide a mensagem em linhas usando '\n', com no máximo `totalLines`
    String msg(message);
    String lines[totalLines];
    int lineCount = 0;
    int start = 0;
    int idx = msg.indexOf('\n');
    while (idx != -1 && lineCount < totalLines) {
        lines[lineCount++] = msg.substring(start, idx);
        start = idx + 1;
        idx = msg.indexOf('\n', start);
    }
    if (lineCount < totalLines && start < msg.length()) {
        lines[lineCount++] = msg.substring(start);
    }
  
    // Exibe cada linha sem espaçamento excessivo
    for (int i = 0; i < lineCount; i++) {
        tft.setTextSize(1);  
        tft.setTextColor(TFT_WHITE);
        tft.drawString(lines[i], x, y);
        y += lineHeight - 2;
    }
}


void ST7789_Handler::clear() {
  tft.fillScreen(TFT_BLACK);
}
