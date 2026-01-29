#include "ST7789_Handler.h"
#include <vector>

ST7789_Handler display;

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
  
  // Acende a luz de fundo manualmente (se necessário)
  pinMode(38, OUTPUT);
  digitalWrite(38, HIGH);  // Garante que a luz de fundo esteja ligada
}

// Sobrecarga para exibir uma string simples
void ST7789_Handler::print(const std::string& message) {
    static std::string lastMessage = ""; // Armazena a última mensagem exibida

    // Se a mensagem não mudou, não redesenha para evitar flickering
    if (lastMessage == message) {
        return;
    }
    lastMessage = message; // Atualiza a mensagem armazenada

    // Limpa a tela apenas quando necessário
    tft.fillScreen(TFT_BLACK);

    const int margin = 2;
    const int availableHeight = 240 - margin;
    const int totalLines = 12;
    const int lineHeight = availableHeight / totalLines;

    int x = 2;
    int y = margin / 2;

    // Divide a string em múltiplas linhas
    String text(message.c_str());
    String lines[totalLines];
    int lineCount = 0;
    int start = 0;
    int idx = text.indexOf('\n');
    while (idx != -1 && lineCount < totalLines) {
        lines[lineCount++] = text.substring(start, idx);
        start = idx + 1;
        idx = text.indexOf('\n', start);
    }
    if (lineCount < totalLines && start < text.length()) {
        lines[lineCount++] = text.substring(start);
    }

    // Exibe cada linha com espaçamento adequado
    for (int i = 0; i < lineCount; i++) {
        tft.setTextSize(1);  
        tft.setTextColor(TFT_WHITE);
        tft.drawString(lines[i], x, y);
        y += lineHeight - 2;
    }
}

// Sobrecarga para exibir um vetor de strings
void ST7789_Handler::print(const std::vector<std::string>& messages) {
    static std::string lastMessage = ""; // Armazena a última mensagem exibida

    // Concatena os elementos do vetor em uma única string separada por ", "
    std::string concatenatedMessage;
    for (size_t i = 0; i < messages.size(); ++i) {
        concatenatedMessage += messages[i];
        if (i < messages.size() - 1) {
            concatenatedMessage += ", ";
        }
    }

    // Se a mensagem não mudou, não redesenha para evitar flickering
    if (lastMessage == concatenatedMessage) {
        return;
    }
    lastMessage = concatenatedMessage; // Atualiza a mensagem armazenada

    // Chamamos a versão da função `print(const std::string&)`
    print(concatenatedMessage);
}

// Sobrecarga para exibir uma string quebrando por vírgula
void ST7789_Handler::println(const std::string& message) {
    static std::string lastMessage = ""; // Armazena a última mensagem exibida

    // Se a mensagem não mudou, não redesenha para evitar flickering
    if (lastMessage == message) {
        return;
    }
    lastMessage = message; // Atualiza a mensagem armazenada

    // Limpa a tela apenas quando necessário
    tft.fillScreen(TFT_BLACK);

    const int margin = 2;
    const int availableHeight = 240 - margin;
    const int totalLines = 12;
    const int lineHeight = availableHeight / totalLines;

    int x = 2;
    int y = margin / 2;

    // Divide a string em linhas quebrando por ","
    String text(message.c_str());
    String lines[totalLines];
    int lineCount = 0;
    int start = 0;
    int idx = text.indexOf(',');
    while (idx != -1 && lineCount < totalLines) {
        lines[lineCount++] = text.substring(start, idx);
        start = idx + 1;
        idx = text.indexOf(',', start);
    }
    if (lineCount < totalLines && start < text.length()) {
        lines[lineCount++] = text.substring(start);
    }

    // Exibe cada linha com espaçamento adequado
    for (int i = 0; i < lineCount; i++) {
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        tft.drawString(lines[i], x, y);  // Removemos .trim(), pois não retorna um valor
        y += lineHeight - 2;
    }
}

// Sobrecarga para exibir um vetor de strings, quebrando cada elemento em uma nova linha
void ST7789_Handler::println(const std::vector<std::string>& messages) {
    static std::string lastMessage = ""; // Armazena a última mensagem exibida

    // Concatena os elementos do vetor para verificar mudança
    std::string concatenatedMessage;
    for (const auto& msg : messages) {
        concatenatedMessage += msg + ",";
    }

    // Se a mensagem não mudou, não redesenha para evitar flickering
    if (lastMessage == concatenatedMessage) {
        return;
    }
    lastMessage = concatenatedMessage; // Atualiza a mensagem armazenada

    // Chamamos a versão da função `println(const std::string&)`
    println(concatenatedMessage);
}

// Apenas uma definição para evitar erro de duplicação
void ST7789_Handler::clear() {
  tft.fillScreen(TFT_BLACK);
}