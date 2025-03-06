#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "USB_Conexion.h"
#include "BLE_Conexion.h"

// Estrutura que representa um evento MIDI com os campos desejados
struct MIDIEventData {
  int index;             // Contador global de eventos
  int msgIndex;          // Índice para vincular NoteOn e NoteOff
  unsigned long tempo;   // Tempo (em milissegundos – usamos millis())
  unsigned long delay;   // Diferença em ms em relação ao evento anterior
  int canal;             // Canal MIDI
  std::string mensagem;  // "NoteOn" ou "NoteOff"
  int nota;              // Número da nota MIDI
  std::string som;       // Nota musical (ex.: "C")
  std::string oitava;    // Nota com oitava (ex.: "C4")
  int velocidade;        // Velocidade
  int blockIndex;        // Índice do bloco (agrupamento de notas simultâneas)
  int flushOff;          // Indica se a nota sofreu flush (0 = normal, 1 = flush)
};

class MIDIHandler {
public:
  MIDIHandler();

  void begin();
  void task();
  void enableHistory(int capacity);
  void addEvent(const MIDIEventData& event);
  void processQueue();
  void setQueueLimit(int maxEvents);
  const std::deque<MIDIEventData>& getQueue() const;

  void handleMidiMessage(const uint8_t* data, size_t length);

  std::string getActiveNotesString() const;
  std::string getActiveNotes() const;
  std::vector<std::string> getActiveNotesVector() const;
  size_t getActiveNotesCount() const;
  void clearActiveNotesNow();

  // Funções auxiliares para manipulação de blocos de eventos:
  // Retorna o maior blockIndex dentre os eventos da fila.
  int lastBlock(const std::deque<MIDIEventData>& queue) const;
  // Filtra os eventos do bloco informado e extrai os campos solicitados.
  // Se 'fields' não for informado ou contiver "all", retorna todos os campos formatados.
  // Retorna os eventos de um bloco específico, podendo escolher se deseja os rótulos nos campos ou apenas os valores
  std::vector<std::string> getBlock(int block, const std::deque<MIDIEventData>& queue, const std::vector<std::string>& fields = { "all" }, bool includeLabels = false) const;
  // Retorna os dados do último bloco MIDI registrado, podendo filtrar um campo específico ou retornar todos os dados do bloco.
  // Retorna os dados do último bloco MIDI registrado, podendo filtrar um campo específico e escolher se deseja rótulos nos campos.
  // Retorna os dados do último bloco MIDI, podendo aceitar uma string única ou um vetor de strings.
  std::vector<std::string> getAnswer(const std::string& field = "all", bool includeLabels = false) const;
  std::vector<std::string> getAnswer(const std::vector<std::string>& fields, bool includeLabels = false) const;



private:
  std::deque<MIDIEventData> eventQueue;
  int maxEvents;
  int globalIndex;
  int nextMsgIndex;
  unsigned long lastTempo;

  std::unordered_map<int, int> activeNotes;
  std::unordered_map<int, int> activeMsgIndex;

  int nextBlockIndex;
  int currentBlockIndex;
  unsigned long lastNoteOffTime;
  static const unsigned long NOTE_TIMEOUT = 1200;

  // Histórico armazenado na PSRAM (buffer dinâmico circular)
  MIDIEventData* historyQueue;
  int historyQueueCapacity;
  int historyQueueSize;
  int historyQueueHead;

  // Expande dinamicamente o buffer circular na PSRAM para evitar perda de eventos
  void expandHistoryQueue();

  void flushActiveNotes(unsigned long currentTime);
  std::string getNoteName(int note) const;
  std::string getNoteWithOctave(int note) const;

  class MyUSB_Conexion : public USB_Conexion {
  public:
    MyUSB_Conexion(MIDIHandler* handler)
      : handler(handler) {}
    virtual void onMidiDataReceived(const uint8_t* data, size_t length) override {
      handler->handleMidiMessage(data, length);
    }
  private:
    MIDIHandler* handler;
  };

  class MyBLE_Conexion : public BLE_Conexion {
  public:
    MyBLE_Conexion(MIDIHandler* handler)
      : handler(handler) {}
    virtual void onMidiDataReceived(const uint8_t* data, size_t length) override {
      handler->handleMidiMessage(data, length);
    }
  private:
    MIDIHandler* handler;
  };

  MyUSB_Conexion usbCon;
  MyBLE_Conexion bleCon;
};

extern MIDIHandler midiHandler;

#endif  // MIDI_HANDLER_H
