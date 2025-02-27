#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <deque>
#include <string>
#include <unordered_map>

// Estrutura que representa um evento MIDI com os campos desejados
struct MIDIEventData {
    int index;            // Contador global de eventos
    int msgIndex;         // Índice para vincular NoteOn e NoteOff
    unsigned long tempo;  // Tempo (em milissegundos – usamos millis())
    unsigned long delay;  // Diferença em ms em relação ao evento anterior
    int canal;            // Canal MIDI
    std::string mensagem; // "NoteOn" ou "NoteOff"
    int nota;             // Número da nota MIDI
    std::string som;      // Nota musical (ex.: "C")
    std::string oitava;   // Nota com oitava (ex.: "C4")
    int velocidade;       // Velocidade
    int blockIndex;       // Índice do bloco (agrupamento de notas simultâneas)
};

class MIDIHandler {
public:
    MIDIHandler();

    // Adiciona um evento à fila e mantém somente os últimos maxEvents na SRAM
    void addEvent(const MIDIEventData& event);
    void processQueue();
    void setQueueLimit(int maxEvents);
    const std::deque<MIDIEventData>& getQueue() const;

    // Métodos para acessar o histórico armazenado na PSRAM
    const MIDIEventData* getHistoryQueue() const; // Retorna ponteiro para o buffer de histórico
    int getHistoryQueueSize() const;              // Retorna número de eventos armazenados no histórico

    // Processa uma mensagem MIDI recebida (aceita 3 ou 4 bytes – se 4, ignora o primeiro byte USB)
    void handleMidiMessage(const uint8_t* data, size_t length);

    // Retorna uma string com o estado atual das notas ativas (para debug)
    std::string getActiveNotesString() const;
    // Retorna a quantidade de notas ativas
    size_t getActiveNotesCount() const;

private:
    std::deque<MIDIEventData> eventQueue;  // Fila de eventos na SRAM (últimos maxEvents)
    int maxEvents;                         // Limite da fila na SRAM (ex.: 1000)
    int globalIndex;
    int nextMsgIndex;
    unsigned long lastTempo;

    // Fila de histórico na PSRAM (buffer circular)
    MIDIEventData* historyQueue;  // Buffer alocado na PSRAM
    int historyQueueCapacity;     // Capacidade do histórico (ex.: 15000)
    int historyQueueSize;         // Número atual de eventos armazenados no histórico
    int historyQueueHead;         // Índice para próxima inserção (buffer circular)

    // Mapas para rastrear o estado das notas ativas
    std::unordered_map<int, int> activeNotes;   // nota -> blockIndex do NoteOn ativo
    std::unordered_map<int, int> activeMsgIndex;  // nota -> msgIndex do NoteOn ativo

    int nextBlockIndex;
    int currentBlockIndex;

    // Tempo do último NoteOff real processado para o bloco atual.
    unsigned long lastNoteOffTime;
    // Timeout para liberar notas que ficaram presas (em milissegundos)
    static const unsigned long NOTE_TIMEOUT = 1200;

    // Gera NoteOff artificial para todas as notas ativas (flush) e reseta o estado.
    void flushActiveNotes(unsigned long currentTime);

    // Funções auxiliares para formatação de notas
    std::string getNoteName(int note) const;
    std::string getNoteWithOctave(int note) const;
};

#endif // MIDI_HANDLER_H
