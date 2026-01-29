#include <Arduino.h>
#include "MIDI_Handler.h"
MIDIHandler midiHandler;

#include <cstdio>
#include <sstream>
#include "esp_heap_caps.h"  // Para alocação em PSRAM

// Definição da constante NOTE_TIMEOUT (1200 ms, conforme definido no header)
const unsigned long MIDIHandler::NOTE_TIMEOUT;

MIDIHandler::MIDIHandler()
  : maxEvents(20),
    globalIndex(0),
    nextMsgIndex(1),
    lastTempo(0),
    nextBlockIndex(1),
    currentBlockIndex(0),
    lastNoteOffTime(0),
    lastNoteOnTime(0),
    historyQueue(nullptr),
    historyQueueCapacity(0),
    historyQueueSize(0),
    historyQueueHead(0),
    usbCon(this),
    bleCon(this) {
  // Por padrão, o histórico está desativado.
}

MIDIHandler::~MIDIHandler() {
  if (historyQueue) {
    // Chama destrutor de cada elemento para liberar std::string corretamente
    for (int i = 0; i < historyQueueCapacity; i++) {
      historyQueue[i].~MIDIEventData();
    }
    free(historyQueue);
    historyQueue = nullptr;
  }
}

void MIDIHandler::begin() {
  // Inicializa as conexões USB e BLE internas
  usbCon.begin();
  bleCon.begin();
}

void MIDIHandler::task() {
  // Processa os eventos das conexões USB e BLE
  usbCon.task();
  bleCon.task();
}

void MIDIHandler::enableHistory(int capacity) {
  if (capacity <= 0) {
    // Desativa o histórico, liberando a memória alocada
    if (historyQueue) {
      free(historyQueue);
      historyQueue = nullptr;
    }
    historyQueueCapacity = 0;
    historyQueueSize = 0;
    historyQueueHead = 0;
    Serial.println("Histórico MIDI desativado!");
    return;
  }

  // Se já existe um histórico, libera a memória antes de realocar
  if (historyQueue) {
    free(historyQueue);
    historyQueue = nullptr;
  }

  // Aloca memória para o histórico na PSRAM
  historyQueue = static_cast<MIDIEventData*>(heap_caps_malloc(capacity * sizeof(MIDIEventData), MALLOC_CAP_SPIRAM));

  if (!historyQueue) {
    Serial.println("Erro ao alocar memória para historyQueue na PSRAM!");
    historyQueueCapacity = 0;
    return;
  }

  historyQueueCapacity = capacity;
  historyQueueSize = 0;
  historyQueueHead = 0;

  // Inicializa cada objeto no buffer usando placement new para evitar problemas com std::string
  for (int i = 0; i < historyQueueCapacity; i++) {
    new (&historyQueue[i]) MIDIEventData();
  }

  Serial.println("Histórico MIDI ativado!");
}


void MIDIHandler::setQueueLimit(int maxEvents) {
  this->maxEvents = maxEvents;
}

const std::deque<MIDIEventData>& MIDIHandler::getQueue() const {
  return eventQueue;
}

// Função privada que expande o buffer dinâmico (ring buffer) na PSRAM.
void MIDIHandler::expandHistoryQueue() {
  // Define nova capacidade (se o atual for 0, define um tamanho inicial, por exemplo, 10)
  int newCapacity = (historyQueueCapacity > 0) ? (historyQueueCapacity * 2) : 10;
  MIDIEventData* newQueue = static_cast<MIDIEventData*>(heap_caps_malloc(newCapacity * sizeof(MIDIEventData), MALLOC_CAP_SPIRAM));
  if (!newQueue) {
    Serial.println("Erro ao expandir o histórico MIDI!");
    return;
  }
  // Calcula o índice da cauda no buffer circular atual.
  int tail = (historyQueueHead + historyQueueCapacity - historyQueueSize) % historyQueueCapacity;
  // Copia os eventos do buffer antigo para o novo, preservando a ordem.
  for (int i = 0; i < historyQueueSize; i++) {
    int index = (tail + i) % historyQueueCapacity;
    newQueue[i] = historyQueue[index];
  }
  // Inicializa os demais elementos do novo buffer (evita problemas com std::string)
  for (int i = historyQueueSize; i < newCapacity; i++) {
    new (&newQueue[i]) MIDIEventData();
  }
  free(historyQueue);
  historyQueue = newQueue;
  historyQueueCapacity = newCapacity;
  // Após copiar os elementos, o novo head é imediatamente após o último elemento.
  historyQueueHead = historyQueueSize;
  Serial.printf("Histórico MIDI expandido para %d eventos!\n", historyQueueCapacity);
}

void MIDIHandler::addEvent(const MIDIEventData& event) {
  // Adiciona o evento na fila principal (armazenada na SRAM)
  eventQueue.push_back(event);
  processQueue();

  // Se o histórico estiver ativo, adiciona o evento no buffer dinâmico na PSRAM
  if (historyQueue != nullptr && historyQueueCapacity > 0) {
    // Se o buffer estiver cheio, expande-o para não descartar nenhum evento
    if (historyQueueSize == historyQueueCapacity) {
      expandHistoryQueue();
    }
    historyQueue[historyQueueHead] = event;
    historyQueueHead = (historyQueueHead + 1) % historyQueueCapacity;
    historyQueueSize++;
  }
}


void MIDIHandler::processQueue() {
  while (eventQueue.size() > static_cast<size_t>(maxEvents)) {
    eventQueue.pop_front();
  }
}

std::string MIDIHandler::getNoteName(int note) const {
  static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  return names[note % 12];
}

std::string MIDIHandler::getNoteWithOctave(int note) const {
  int octave = (note / 12) - 1;
  return getNoteName(note) + std::to_string(octave);
}


std::string MIDIHandler::getActiveNotes() const {
  std::ostringstream oss;
  oss << "{";

  // Passo 1: Criar um vetor com as chaves do mapa (números das notas)
  std::vector<int> sortedNotes;
  for (const auto& kv : activeNotes) {
    sortedNotes.push_back(kv.first);
  }

  // Passo 2: Ordenar o vetor
  std::sort(sortedNotes.begin(), sortedNotes.end());

  // Passo 3: Construir a string ordenada
  bool first = true;
  for (int note : sortedNotes) {
    if (!first) oss << ", ";
    oss << getNoteName(note);  // Pega o bloco correspondente
    first = false;
  }

  oss << "}";
  return oss.str();
}

std::vector<std::string> MIDIHandler::getActiveNotesVector() const {
  std::vector<std::string> activeNotesVector;

  // Usamos std::map para garantir a ordenação automática das notas MIDI
  std::map<int, int> sortedActiveNotes(activeNotes.begin(), activeNotes.end());

  // Percorre o mapa ordenado e adiciona as notas ao vetor
  for (const auto& kv : sortedActiveNotes) {
    activeNotesVector.push_back(getNoteName(kv.first));
  }

  return activeNotesVector;
}

std::string MIDIHandler::getActiveNotesString() const {
  std::ostringstream oss;
  oss << "{";

  // Usamos um std::map para manter as notas ordenadas automaticamente
  std::map<int, int> sortedActiveNotes(activeNotes.begin(), activeNotes.end());

  bool first = true;
  for (const auto& kv : sortedActiveNotes) {
    if (!first) oss << ", ";
    oss << getNoteName(kv.first) << ", {" << kv.second << "}";
    first = false;
  }

  oss << "}";
  return oss.str();
}


size_t MIDIHandler::getActiveNotesCount() const {
  return activeNotes.size();
}

void MIDIHandler::flushActiveNotes(unsigned long currentTime) {
  for (auto const& entry : activeNotes) {
    int note = entry.first;
    int block = entry.second;
    MIDIEventData event;
    event.index = ++globalIndex;
    event.msgIndex = activeMsgIndex[note];
    event.tempo = currentTime;
    event.delay = (globalIndex == 1) ? 0 : (currentTime - lastTempo);
    lastTempo = currentTime;
    event.canal = 1;  // Canal fixo para flush; ajuste se necessário.
    event.mensagem = "NoteOff";
    event.nota = note;
    event.som = getNoteName(note);
    event.oitava = getNoteWithOctave(note);
    event.velocidade = 0;
    event.blockIndex = block;
    event.flushOff = 1;  // Indica que essa nota foi desligada por um flush
    event.pitchBend = 0;
    addEvent(event);
  }
  activeNotes.clear();
  activeMsgIndex.clear();
  currentBlockIndex = 0;
  lastNoteOffTime = 0;
  lastNoteOnTime = 0;
}

// Limpa as notas ativas
void MIDIHandler::clearActiveNotesNow() {
  activeNotes.clear();
  activeMsgIndex.clear();
  currentBlockIndex = 0;
  lastNoteOffTime = 0;
}

int MIDIHandler::lastBlock(const std::deque<MIDIEventData>& queue) const {
  int maxBlock = 0;
  for (const auto& event : queue) {
    if (event.blockIndex > maxBlock) {
      maxBlock = event.blockIndex;
    }
  }
  return maxBlock;
}

std::vector<std::string> MIDIHandler::getBlock(int block, const std::deque<MIDIEventData>& queue, const std::vector<std::string>& fields, bool includeLabels) const {
  std::vector<MIDIEventData> blockEvents;

  // Filtra apenas os eventos NoteOn do bloco especificado
  for (const auto& event : queue) {
    if (event.blockIndex == block && event.mensagem == "NoteOn") {
      blockEvents.push_back(event);
    }
  }

  // Ordena os eventos por número de nota (campo "nota")
  std::sort(blockEvents.begin(), blockEvents.end(), [](const MIDIEventData& a, const MIDIEventData& b) {
    return a.nota < b.nota;
  });

  std::vector<std::string> result;

  // Se "all" foi solicitado, retorna todos os campos
  if (fields.size() == 1 && fields[0] == "all") {
    for (const auto& event : blockEvents) {
      std::ostringstream oss;
      if (includeLabels) {  // Com rótulos
        oss << "{index:" << event.index
            << ", msgIndex:" << event.msgIndex
            << ", tempo:" << event.tempo
            << ", delay:" << event.delay
            << ", canal:" << event.canal
            << ", mensagem:" << event.mensagem
            << ", nota:" << event.nota
            << ", som:" << event.som
            << ", oitava:" << event.oitava
            << ", velocidade:" << event.velocidade
            << ", blockIndex:" << event.blockIndex
            << ", flushOff:" << event.flushOff
            << ", pitchBend:" << event.pitchBend << "}";
      } else {  // Sem rótulos
        oss << "{ " << event.index
            << ", " << event.msgIndex
            << ", " << event.tempo
            << ", " << event.delay
            << ", " << event.canal
            << ", " << event.mensagem
            << ", " << event.nota
            << ", " << event.som
            << ", " << event.oitava
            << ", " << event.velocidade
            << ", " << event.blockIndex
            << ", " << event.flushOff
            << ", " << event.pitchBend << " }";
      }
      result.push_back(oss.str());
    }
  }
  // Se apenas um campo foi solicitado
  else if (fields.size() == 1) {
    std::string field = fields[0];
    for (const auto& event : blockEvents) {
      if (field == "som") {
        result.push_back(event.som);
      } else if (field == "oitava" || field == "octave") {
        result.push_back(event.oitava);
      } else if (field == "mensagem") {
        result.push_back(event.mensagem);
      } else if (field == "nota") {
        result.push_back(std::to_string(event.nota));
      } else if (field == "tempo") {
        result.push_back(std::to_string(event.tempo));
      } else if (field == "velocidade") {
        result.push_back(std::to_string(event.velocidade));
      } else if (field == "canal") {
        result.push_back(std::to_string(event.canal));
      } else if (field == "pitchBend") {
        result.push_back(std::to_string(event.pitchBend));
      }
    }
  }
  // Se mais de um campo foi solicitado
  else {
    for (const auto& event : blockEvents) {
      std::ostringstream oss;
      bool first = true;
      for (const auto& field : fields) {
        if (!first) oss << ", ";
        if (field == "som") {
          oss << event.som;
        } else if (field == "oitava" || field == "octave") {
          oss << event.oitava;
        } else if (field == "mensagem") {
          oss << event.mensagem;
        } else if (field == "nota") {
          oss << event.nota;
        } else if (field == "tempo") {
          oss << event.tempo;
        } else if (field == "velocidade") {
          oss << event.velocidade;
        } else if (field == "canal") {
          oss << event.canal;
        } else if (field == "pitchBend") {
          oss << event.pitchBend;
        }
        first = false;
      }
      result.push_back(oss.str());
    }
  }

  return result;
}

std::vector<std::string> MIDIHandler::getAnswer(const std::string& field, bool includeLabels) const {
  return getAnswer(std::vector<std::string>{ field }, includeLabels);
}

std::vector<std::string> MIDIHandler::getAnswer(const std::vector<std::string>& fields, bool includeLabels) const {
  std::vector<std::string> result;
  const std::deque<MIDIEventData>& queue = getQueue();

  if (!queue.empty()) {
    int lastBlockIdx = lastBlock(queue);

    // Passa o vetor de campos diretamente para getBlock()
    result = getBlock(lastBlockIdx, queue, fields, includeLabels);
  }

  return result;
}


void MIDIHandler::handleMidiMessage(const uint8_t* data, size_t length) {
  const uint8_t* midiData = (length == 3) ? data : (length >= 4 ? data + 1 : nullptr);
  if (midiData == nullptr) return;

  unsigned long tempo = millis();

  // Timeout de notas: verifica tanto após NoteOff quanto após NoteOn sem atividade
  if (!activeNotes.empty()) {
    unsigned long lastActivity = (lastNoteOffTime > lastNoteOnTime) ? lastNoteOffTime : lastNoteOnTime;
    if (lastActivity != 0 && (tempo - lastActivity >= NOTE_TIMEOUT)) {
      flushActiveNotes(tempo);
    }
  }

  uint8_t midiStatus = midiData[0] & 0xF0;
  int canal = (midiData[0] & 0x0F) + 1;
  unsigned long diff = (globalIndex == 0) ? 0 : (tempo - lastTempo);
  lastTempo = tempo;

  // Mensagens de canal que não são NoteOn/NoteOff
  if (midiStatus == 0xB0) {  // Control Change
    MIDIEventData event;
    event.index = ++globalIndex;
    event.msgIndex = 0;
    event.tempo = tempo;
    event.delay = diff;
    event.canal = canal;
    event.mensagem = "CC";
    event.nota = midiData[1];        // Número do controlador
    event.som = "";
    event.oitava = "";
    event.velocidade = midiData[2];  // Valor do CC
    event.blockIndex = currentBlockIndex;
    event.flushOff = 0;
    event.pitchBend = 0;
    addEvent(event);
    return;
  }

  if (midiStatus == 0xC0) {  // Program Change
    MIDIEventData event;
    event.index = ++globalIndex;
    event.msgIndex = 0;
    event.tempo = tempo;
    event.delay = diff;
    event.canal = canal;
    event.mensagem = "ProgramChange";
    event.nota = midiData[1];  // Número do programa
    event.som = "";
    event.oitava = "";
    event.velocidade = 0;
    event.blockIndex = currentBlockIndex;
    event.flushOff = 0;
    event.pitchBend = 0;
    addEvent(event);
    return;
  }

  if (midiStatus == 0xD0) {  // Channel Pressure (Aftertouch)
    MIDIEventData event;
    event.index = ++globalIndex;
    event.msgIndex = 0;
    event.tempo = tempo;
    event.delay = diff;
    event.canal = canal;
    event.mensagem = "ChannelPressure";
    event.nota = 0;
    event.som = "";
    event.oitava = "";
    event.velocidade = midiData[1];  // Valor de pressão
    event.blockIndex = currentBlockIndex;
    event.flushOff = 0;
    event.pitchBend = 0;
    addEvent(event);
    return;
  }

  if (midiStatus == 0xE0) {  // Pitch Bend
    int pitchValue = (midiData[1] & 0x7F) | ((midiData[2] & 0x7F) << 7);
    MIDIEventData event;
    event.index = ++globalIndex;
    event.msgIndex = 0;
    event.tempo = tempo;
    event.delay = diff;
    event.canal = canal;
    event.mensagem = "PitchBend";
    event.nota = 0;
    event.som = "";
    event.oitava = "";
    event.velocidade = 0;
    event.blockIndex = currentBlockIndex;
    event.flushOff = 0;
    event.pitchBend = pitchValue;
    addEvent(event);
    return;
  }

  // NoteOn / NoteOff
  int note = midiData[1];
  int velocity = midiData[2];
  std::string mensagem;
  int msgIndex = 0;
  int blockIdx = currentBlockIndex;

  if (midiStatus == 0x90) {  // NoteOn
    if (velocity > 0) {
      mensagem = "NoteOn";
      msgIndex = nextMsgIndex++;
      if (activeNotes.empty()) {
        currentBlockIndex = nextBlockIndex++;
      }
      blockIdx = currentBlockIndex;
      activeNotes[note] = currentBlockIndex;
      activeMsgIndex[note] = msgIndex;
      lastNoteOnTime = tempo;
    } else {  // NoteOn com velocity 0 equivale a NoteOff
      mensagem = "NoteOff";
      auto it = activeNotes.find(note);
      if (it != activeNotes.end()) {
        blockIdx = it->second;
        msgIndex = activeMsgIndex[note];
        activeNotes.erase(it);
        activeMsgIndex.erase(note);
      } else {
        blockIdx = currentBlockIndex;
      }
      lastNoteOffTime = tempo;
    }
  } else if (midiStatus == 0x80) {  // NoteOff
    mensagem = "NoteOff";
    auto it = activeNotes.find(note);
    if (it != activeNotes.end()) {
      blockIdx = it->second;
      msgIndex = activeMsgIndex[note];
      activeNotes.erase(it);
      activeMsgIndex.erase(note);
    } else {
      blockIdx = currentBlockIndex;
    }
    lastNoteOffTime = tempo;
  } else {
    return;  // Mensagem MIDI não reconhecida
  }

  if (activeNotes.empty()) {
    currentBlockIndex = 0;
    lastNoteOffTime = 0;
    lastNoteOnTime = 0;
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
  event.flushOff = 0;
  event.pitchBend = 0;

  addEvent(event);
}
