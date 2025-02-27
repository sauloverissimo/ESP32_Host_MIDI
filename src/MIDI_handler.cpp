#include <Arduino.h>
#include "MIDI_Handler.h"
#include <cstdio>
#include <sstream>
#include "esp_heap_caps.h"  // Para alocação em PSRAM

// Constante para timeout em milissegundos.
const unsigned long MIDIHandler::NOTE_TIMEOUT; // definido como 1200 ms no header

// Construtor: inicializa contadores, limites e aloca a memória para o histórico na PSRAM.
MIDIHandler::MIDIHandler()
: maxEvents(1000),
  globalIndex(0),
  nextMsgIndex(1),
  lastTempo(0),
  nextBlockIndex(1),
  currentBlockIndex(0),
  lastNoteOffTime(0),
  historyQueue(nullptr),
  historyQueueCapacity(15000),
  historyQueueSize(0),
  historyQueueHead(0)
{
    // Aloca o buffer para o histórico na PSRAM
    historyQueue = (MIDIEventData*) heap_caps_malloc(historyQueueCapacity * sizeof(MIDIEventData), MALLOC_CAP_SPIRAM);
    if (!historyQueue) {
        Serial.println("Erro ao alocar memória para historyQueue na PSRAM!");
        historyQueueCapacity = 0;
    } else {
        // Constrói cada objeto no buffer usando placement new para garantir que os std::string sejam inicializados
        for (int i = 0; i < historyQueueCapacity; i++) {
            new (&historyQueue[i]) MIDIEventData();
        }
    }
}

void MIDIHandler::setQueueLimit(int maxEvents) {
    this->maxEvents = maxEvents;
}

const std::deque<MIDIEventData>& MIDIHandler::getQueue() const {
    return eventQueue;
}

const MIDIEventData* MIDIHandler::getHistoryQueue() const {
    return historyQueue;
}

int MIDIHandler::getHistoryQueueSize() const {
    return historyQueueSize;
}

void MIDIHandler::addEvent(const MIDIEventData& event) {
    // Adiciona na fila primária (SRAM)
    eventQueue.push_back(event);
    processQueue();

    // Adiciona no histórico (PSRAM) se a memória foi alocada
    if (historyQueueCapacity > 0 && historyQueue != nullptr) {
        historyQueue[historyQueueHead] = event;
        historyQueueHead = (historyQueueHead + 1) % historyQueueCapacity;
        if (historyQueueSize < historyQueueCapacity) {
            historyQueueSize++;
        }
    }
}

void MIDIHandler::processQueue() {
    while (eventQueue.size() > static_cast<size_t>(maxEvents)) {
         eventQueue.pop_front();
    }
}

std::string MIDIHandler::getNoteName(int note) const {
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return names[note % 12];
}

std::string MIDIHandler::getNoteWithOctave(int note) const {
    int octave = (note / 12) - 1;
    return getNoteName(note) + std::to_string(octave);
}

std::string MIDIHandler::getActiveNotesString() const {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (auto const &kv : activeNotes) {
        if (!first) oss << ", ";
        oss << getNoteName(kv.first) << "(" << kv.second << ")";
        first = false;
    }
    oss << "]";
    return oss.str();
}

size_t MIDIHandler::getActiveNotesCount() const {
    return activeNotes.size();
}

// Gera eventos NoteOff artificiais para liberar as notas que ficaram ativas.
// Após o flush, activeNotes é esvaziado e lastNoteOffTime é reiniciado (zero).
void MIDIHandler::flushActiveNotes(unsigned long currentTime) {
    for (auto const &entry : activeNotes) {
        int note = entry.first;
        int block = entry.second;
        MIDIEventData event;
        event.index = ++globalIndex;
        event.msgIndex = activeMsgIndex[note];
        event.tempo = currentTime;
        event.delay = (globalIndex == 1) ? 0 : (currentTime - lastTempo);
        lastTempo = currentTime;
        event.canal = 1; // Canal fixo para flush; ajuste se necessário.
        event.mensagem = "NoteOff";
        event.nota = note;
        event.som = getNoteName(note);
        event.oitava = getNoteWithOctave(note);
        event.velocidade = 0;
        event.blockIndex = block;
        addEvent(event);
    }
    activeNotes.clear();
    activeMsgIndex.clear();
    currentBlockIndex = 0;
    // Reinicia o lastNoteOffTime, sinalizando que o bloco foi completamente liberado.
    lastNoteOffTime = 0;
}

// Processa uma mensagem MIDI recebida.
void MIDIHandler::handleMidiMessage(const uint8_t* data, size_t length) {
    // Se a mensagem tem 3 bytes, já é MIDI; se 4 ou mais, ignora o primeiro byte (cabeçalho USB).
    const uint8_t* midiData = (length == 3) ? data : (length >= 4 ? data + 1 : nullptr);
    if (midiData == nullptr) return;

    unsigned long tempo = millis();

    // Se houver notas ativas e se lastNoteOffTime for diferente de zero
    // e se (tempo - lastNoteOffTime) for maior ou igual ao timeout, executa flush.
    if (!activeNotes.empty() && (lastNoteOffTime != 0) && (tempo - lastNoteOffTime >= NOTE_TIMEOUT)) {
        flushActiveNotes(tempo);
    }

    uint8_t midiStatus = midiData[0] & 0xF0;
    int note = midiData[1];
    int velocity = midiData[2];
    int canal = (midiData[0] & 0x0F) + 1;
    unsigned long diff = (globalIndex == 0) ? 0 : (tempo - lastTempo);
    lastTempo = tempo;
    std::string mensagem;
    int msgIndex = 0;
    int blockIdx = 0;
    
    if (midiStatus == 0x90) { // NoteOn
        if (velocity > 0) {
            mensagem = "NoteOn";
            msgIndex = nextMsgIndex++;
            // Se não há nenhuma nota ativa, inicia novo bloco.
            if (activeNotes.empty()) {
                currentBlockIndex = nextBlockIndex++;
            }
            blockIdx = currentBlockIndex;
            // Registra a nota no mesmo bloco.
            activeNotes[note] = currentBlockIndex;
            activeMsgIndex[note] = msgIndex;
            // NoteOn não atualiza lastNoteOffTime.
        } else { // NoteOn com velocity 0 equivale a NoteOff
            mensagem = "NoteOff";
            if (activeNotes.find(note) != activeNotes.end()) {
                blockIdx = activeNotes[note];
                msgIndex = activeMsgIndex[note];
                activeNotes.erase(note);
                activeMsgIndex.erase(note);
            } else {
                return;
            }
            lastNoteOffTime = tempo; // Atualiza o tempo do último NoteOff real.
        }
    } else if (midiStatus == 0x80) { // NoteOff
        mensagem = "NoteOff";
        if (activeNotes.find(note) != activeNotes.end()) {
            blockIdx = activeNotes[note];
            msgIndex = activeMsgIndex[note];
            activeNotes.erase(note);
            activeMsgIndex.erase(note);
        } else {
            return;
        }
        lastNoteOffTime = tempo; // Atualiza o tempo do último NoteOff real.
    } else {
        return;
    }
    
    if (activeNotes.empty()) {
        currentBlockIndex = 0;
        // Se não houver mais notas ativas, reseta lastNoteOffTime.
        lastNoteOffTime = 0;
    }
    
    MIDIEventData event;
    event.index = ++globalIndex;
    event.msgIndex = msgIndex;
    event.tempo = tempo;
    event.delay = diff;
    event.canal = canal;
    event.mensagem = mensagem;
    event.nota = note;
    event.som = getNoteName(note);
    event.oitava = getNoteWithOctave(note);
    event.velocidade = velocity;
    event.blockIndex = blockIdx;
    
    addEvent(event);
}
