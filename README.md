# ESP32_Host_MIDI 🎹📡

![ESP32 Host MIDI](https://via.placeholder.com/320x170.png?text=T-Display+S3)  
*Receba, interprete e exiba mensagens MIDI em tempo real no T-Display S3!*

---

## 📚 Visão Geral

A **ESP32_Host_MIDI** é uma biblioteca desenvolvida para:
- **Receber mensagens MIDI** via USB usando um ESP32 (especialmente o ESP32-S3).
- **Interpretar e formatar** os dados MIDI em diversos formatos (raw, short, note number, message type, etc.) utilizando o módulo **MIDI_handler**.
- **Exibir as mensagens** formatadas no T-Display S3, por meio do **DisplayHandler** (baseado na [LovyanGFX](https://github.com/lovyan03/LovyanGFX)).

A biblioteca é **modular** e permite uma fácil adaptação para outros hardwares, bastando ajustar os arquivos de configuração.

---

## 🚀 Estrutura dos Arquivos

### 1. ESP32_Host_MIDI_Config.h
- **Propósito:** Define a configuração de hardware (pinos) para a comunicação USB MIDI e para o display.
- **Principais definições:**
  - `USB_DP_PIN`, `USB_DN_PIN`: Pinos de dados USB.
  - `TFT_CS_PIN`, `TFT_DC_PIN`, `TFT_RST_PIN`, `TFT_BL_PIN`: Pinos para o display ST7789 (T-Display S3).

---

### 2. ESP32_Host_MIDI.h & ESP32_Host_MIDI.cpp
- **Propósito:** Gerencia a comunicação USB MIDI no ESP32.
- **Principais funções:**
  - **`begin()`**: Inicializa o USB Host e registra o cliente.
  - **`task()`**: Processa os eventos USB e submete as transferências.
  - **`onMidiMessage(const uint8_t *data, size_t length)`**: Função virtual acionada quando uma mensagem MIDI é recebida. Deve ser sobrescrita para processar a mensagem (ex.: converter com o **MIDI_handler** e exibir no display).
- **Detalhes:**
  - As funções auxiliares `_clientEventCallback`, `_onReceive` e `_processConfig` gerenciam eventos e configurações do USB.

---

### 3. MIDI_handler.h & MIDI_handler.cpp
- **Propósito:** Fornece funções estáticas para interpretar a mensagem MIDI "bruta" (após remover o cabeçalho USB) em vários formatos.
- **Formatos Suportados:**
  - **Raw Format:** Ex.: `[0x90, 0x3C, 0x64]`
  - **Short Format:** Ex.: `"90 3C 64"`
  - **Note Number:** Ex.: `"60"`
  - **Message Type:** Ex.: `"NoteOn"`, `"NoteOff"`, `"Control Change"`, `"Program Change"`, etc.
  - **Message Status:** Ex.: `"9n"`
  - **Note Sound com Octave:** Ex.: `"C5"` (ou outro, conforme convenção)
  - **Message Vector:** Estrutura de vetor com os detalhes da mensagem.
- **Uso:** Essas funções são chamadas pela implementação sobrescrita de `onMidiMessage` na classe derivada para formatar os dados antes de exibi-los.

---

### 4. datahandler.h
- **Propósito:** Define tipos de dados para manipulação estruturada dos dados (como tabelas e vetores).
- **Principais Tipos:**
  - `TypeElement`: Pode ser int, double, float, string ou vector de strings.
  - `TypeVector`, `TypeTable`, `TypeCube`: Estruturas para manipulação de coleções de dados.
- **Uso:** Utilizado internamente pelo **MIDI_handler** para criar representações estruturadas (como o vetor de mensagem).

---

### 5. displayhandler.h & displayhandler.cpp
- **Propósito:** Gerencia o display T-Display S3 usando a LovyanGFX.
- **Funcionalidades:**
  - **Rotação:** Gira o display 180° para a orientação horizontal.
  - **Dimensionamento:** Configura o display para 320x170 pixels.
  - **Formatação:** Exibe a mensagem em 5 linhas:
    - **Linha 1:** Raw
    - **Linha 2:** Short
    - **Linha 3:** Note#
    - **Linha 4:** Msg
    - **Linha 5:** Octave (destacada com uma linha separadora, fonte maior e em vermelho)
- **Funções-Chave:**
  - **`init()`**: Inicializa o display com os parâmetros corretos.
  - **`printMidiMessage(const char* message)`**: Exibe a mensagem formatada.
  - **`clear()`**: Limpa o display.

---

### 6. ESP32_Host_MIDI.ino
- **Propósito:** Exemplo de implementação no T-Display S3.
- **Funcionalidade:**
  - Instancia um **DisplayHandler** e uma classe derivada **MyESP32_Host_MIDI** que sobrescreve `onMidiMessage` para:
    - Remover o cabeçalho USB (primeiro byte).
    - Utilizar as funções do **MIDI_handler** para formatar a mensagem.
    - Exibir a mensagem formatada no display.
  - Implementa um timeout de 1 segundo para limpar o display após a exibição.

---

## 🔧 Como Funciona

1. **Recepção MIDI:**  
   Ao conectar um dispositivo MIDI, o ESP32 recebe os dados USB.  
   O **ESP32_Host_MIDI** processa os eventos e, quando uma mensagem é recebida, chama `onMidiMessage`.

2. **Interpretação dos Dados:**  
   A classe derivada (ex.: **MyESP32_Host_MIDI**) remove o cabeçalho USB (0x09) e usa as funções do **MIDI_handler** para converter os bytes em formatos legíveis (raw, short, note number, etc).

3. **Exibição no Display:**  
   O **DisplayHandler** recebe a string formatada e a exibe no T-Display S3:
   - Texto distribuído em 5 linhas.
   - A última linha (Octave) é destacada com uma linha separadora, fonte maior e cor vermelha.
   - O display é rotacionado 180° para a orientação correta.
   - Após 1 segundo, o display é limpo automaticamente.

---

## 🎨 Layout Visual

- **Orientação:** Horizontal (320 x 170 pixels), com 180° de rotação.
- **Fonte Geral:** Tamanho pequeno (ajustável para melhor legibilidade).
- **Linha Destacada:** A última linha ("Octave") é exibida com:
  - **Fonte maior** (tamanho 2).
  - **Cor Vermelha** (ex.: `TFT_RED`).
  - **Separador:** Uma linha horizontal separa a última linha das demais.

---

## 💡 Exemplo de Implementação

```cpp
#include <Arduino.h>
#include "ESP32_Host_MIDI.h"   // Biblioteca USB MIDI
#include "displayhandler.h"    // Handler para exibição no display LovyanGFX
#include "MIDI_handler.h"      // Módulo para interpretar as mensagens MIDI

#define DISPLAY_TIMEOUT 1000  // Tempo de exibição: 1 segundo

unsigned long lastMsgTime = 0;
bool msgDisplayed = false;

// Classe derivada que processa a mensagem MIDI usando o MIDI_handler
class MyESP32_Host_MIDI : public ESP32_Host_MIDI {
public:
  MyESP32_Host_MIDI(DisplayHandler* disp) : display(disp) {}

  // Sobrescreve onMidiMessage para interpretar e exibir a mensagem
  void onMidiMessage(const uint8_t *data, size_t length) override {
    // IMPORTANTE: Remove o cabeçalho USB (primeiro byte)
    const uint8_t* midiData = data + 1;
    size_t midiLength = (length > 1) ? (length - 1) : 0;

    std::string rawStr            = MIDIHandler::getRawFormat(midiData, midiLength);
    std::string shortStr          = MIDIHandler::getShortFormat(midiData, midiLength);
    std::string noteNumStr        = MIDIHandler::getNoteNumberFormat(midiData, midiLength);
    std::string messageStr        = MIDIHandler::getMessageFormat(midiData, midiLength);
    std::string statusStr         = MIDIHandler::getMessageStatusFormat(midiData, midiLength);
    std::string noteSoundOctaveStr= MIDIHandler::getNoteSoundOctave(midiData, midiLength);
    
    // Cria a mensagem dividida em 5 linhas:
    String displayMsg = "";
    displayMsg += "Raw: "      + String(rawStr.c_str())       + "\n";
    displayMsg += "Short: "    + String(shortStr.c_str())     + "\n";
    displayMsg += "Note#: "    + String(noteNumStr.c_str())   + "\n";
    displayMsg += "Msg: "      + String(messageStr.c_str())   + "\n";
    displayMsg += "Octave: "   + String(noteSoundOctaveStr.c_str());
    
    if (display) {
      display->printMidiMessage(displayMsg.c_str());
    }
    
    lastMsgTime = millis();
    msgDisplayed = true;
  }

private:
  DisplayHandler* display;
};

DisplayHandler display;
MyESP32_Host_MIDI usbMidi(&display);

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32_Host_MIDI com interpretação MIDI e display");
  
  display.init();
  usbMidi.begin();
}

void loop() {
  usbMidi.task();
  
  if (msgDisplayed && (millis() - lastMsgTime > DISPLAY_TIMEOUT)) {
    display.clear();
    msgDisplayed = false;
  }
  
  delay(10);
}
