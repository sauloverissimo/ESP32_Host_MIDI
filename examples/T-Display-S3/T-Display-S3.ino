#include "ESP32_Pin_Config.h"
#include "ST7789_Handler.h"
#include "MIDI_Handler.h"
#include "USB_Conexion.h"
#include "BLE_Conexion.h"

// Instância global do MIDIHandler – deve ser criada antes das sub-classes
MIDIHandler midiHandler;

// Subclasse de USB_Conexion que encaminha os dados para o MIDIHandler.
class MyUSB_Conexion : public USB_Conexion {
public:
  virtual void onMidiDataReceived(const uint8_t* data, size_t length) override {
    midiHandler.handleMidiMessage(data, length);
  }
};

// Subclasse de BLE_Conexion que encaminha os dados para o MIDIHandler.
class MyBLE_Conexion : public BLE_Conexion {
public:
  virtual void onMidiDataReceived(const uint8_t* data, size_t length) override {
    midiHandler.handleMidiMessage(data, length);
  }
};

// Instâncias globais das conexões
MyUSB_Conexion usbCon;
MyBLE_Conexion bleCon;

// Instância global do display
ST7789_Handler display;

void setup() {
  // Inicializa Serial (apenas se necessário para depuração externa)
  Serial.begin(115200);

  display.init();
  display.printMessage("Display OK...", 0, 0);
  delay(500);

  // Inicializa a conexão USB
  usbCon.begin();
  // Inicializa a conexão BLE
  bleCon.begin();

  display.printMessage("USB & BLE MIDI Iniciado", 0, 0);
  delay(500);
}

void loop() {
  // Processa os eventos USB e BLE – os dados recebidos serão encaminhados para o MIDIHandler
  usbCon.task();
  bleCon.task();
    
  // Exibe a fila de eventos MIDI processados pelo MIDIHandler
  static String ultimaMsg;
  const auto& queue = midiHandler.getQueue();
  String log;
  if(queue.empty()){
    log = "[Press any key to start...]\n[Aperte uma tecla para iniciar...]";
  }
  else {
    // Exibe o estado atual das notas ativas
    std::string active = midiHandler.getActiveNotesString();
    log += "[" + String(midiHandler.getActiveNotesCount()) + "] " + String(active.c_str()) + "\n";
    int count = 0;
    // Exibe os últimos 12 eventos, do mais recente para o mais antigo
    for(auto it = queue.rbegin(); it != queue.rend() && count < 12; ++it, ++count){
      char line[200];
      sprintf(line, "%d;%d;%lu;%lu;%d;%s;%d;%s;%s;%d;%d",
              it->index, it->msgIndex, it->tempo, it->delay, it->canal,
              it->mensagem.c_str(), it->nota, it->som.c_str(), it->oitava.c_str(),
              it->velocidade, it->blockIndex);
      log += String(line) + "\n";
    }
  }
  display.printMessage(log.c_str(), 0, 0);
  delayMicroseconds(1);
}
