# 🎵 Notas Ativas

O `MIDIHandler` mantém um mapa interno de quais notas estão atualmente pressionadas (NoteOn recebido mas NoteOff ainda não). Diversas APIs permitem consultar esse estado.

---

## API de Notas Ativas

### getActiveNotes() — String formatada

Retorna uma string com todas as notas ativas:

```cpp
std::string ativas = midiHandler.getActiveNotes();
Serial.println(ativas.c_str());
// Exemplo: "{C4, E4, G4}"
```

### getActiveNotesVector() — Lista de strings

Retorna um vetor com cada nota como string:

```cpp
auto notas = midiHandler.getActiveNotesVector();
// Exemplo: ["C4", "E4", "G4"]

for (const auto& n : notas) {
    Serial.println(n.c_str());
}
```

### getActiveNotesCount() — Quantidade

Número de notas atualmente pressionadas:

```cpp
size_t count = midiHandler.getActiveNotesCount();
Serial.printf("%d notas ativas\n", (int)count);
```

### fillActiveNotes() — Array booleano

Preenche um array de 128 posições (uma por nota MIDI):

```cpp
bool ativas[128] = {false};
midiHandler.fillActiveNotes(ativas);

// Verificar nota específica:
if (ativas[60]) Serial.println("C4 está pressionada!");

// Listar todas as ativas:
for (int i = 0; i < 128; i++) {
    if (ativas[i]) {
        Serial.printf("Nota %d ativa\n", i);
    }
}
```

---

## Exemplo Completo

```cpp
#include <ESP32_Host_MIDI.h>
#include <USBConnection.h>      // v6.0+: cada transport explícito
// Tools > USB Mode → "USB Host"

USBConnection usbHost;

void setup() {
    Serial.begin(115200);
    midiHandler.addTransport(&usbHost);
    usbHost.begin();
    midiHandler.begin();
}

void loop() {
    midiHandler.task();

    // Mostrar notas ativas quando algo mudar
    static size_t lastCount = 0;
    size_t count = midiHandler.getActiveNotesCount();

    if (count != lastCount) {
        lastCount = count;

        if (count == 0) {
            Serial.println("[ sem notas ]");
        } else {
            Serial.printf("[%d notas] %s\n",
                (int)count,
                midiHandler.getActiveNotes().c_str());
        }
    }
}
```

Saída ao tocar um acorde de Dó maior e soltar:

```
[1 notas] {C4}
[2 notas] {C4, E4}
[3 notas] {C4, E4, G4}
[2 notas] {C4, E4}
[1 notas] {C4}
[ sem notas ]
```

---

## Usar para Síntese de Áudio

O `fillActiveNotes()` é ideal para síntese em tempo real — copie o array uma vez por frame:

```cpp
bool notas[128];
midiHandler.fillActiveNotes(notas);

// Sintetizar todas as notas ativas
for (int i = 0; i < 128; i++) {
    if (notas[i]) {
        float freq = 440.0f * powf(2.0f, (i - 69) / 12.0f);
        // addOscillator(freq);
    }
}
```

---

## Piano Roll com notas ativas

```cpp
#include <ESP32_Host_MIDI.h>

// Mostrar quais teclas estão pressionadas (ASCII art)
void printPianoRoll() {
    bool notas[128];
    midiHandler.fillActiveNotes(notas);

    // Teclas C4 a B4 (60–71)
    for (int i = 60; i <= 71; i++) {
        Serial.print(notas[i] ? "█" : "░");
    }
    Serial.println();
}

void loop() {
    midiHandler.task();
    printPianoRoll();
    delay(50);
}
```

---

## Limpar Notas Ativas

Em alguns casos (desconexão, reset) você precisa limpar o estado:

```cpp
// Zera o mapa interno de notas ativas
midiHandler.clearActiveNotesNow();
```

---

## getActiveNotesString() — Alternativa

Alias de `getActiveNotes()` com nome alternativo:

```cpp
std::string s = midiHandler.getActiveNotesString();
// Idêntico a getActiveNotes()
```

---

## Monitorar Pressão de Teclas Específicas

Combine `fillActiveNotes()` com uma lista de notas de interesse:

```cpp
const int ACORDE_CM[] = {60, 64, 67};  // C, E, G

bool notas[128];
midiHandler.fillActiveNotes(notas);

bool acordeCompleto = notas[60] && notas[64] && notas[67];
if (acordeCompleto) {
    Serial.println("Dó maior pressionado!");
}
```

---

## Próximos Passos

- [Detecção de Acordes →](deteccao-acordes.md) — agrupar notas em acordes
- [GingoAdapter →](gingo-adapter.md) — identificar nome do acorde
- [Histórico PSRAM →](historico-psram.md) — guardar histórico de notas
