# 🎵 Active Notes

The `MIDIHandler` maintains an internal map of which notes are currently pressed (NoteOn received but NoteOff not yet). Several APIs allow you to query this state.

---

## Active Notes API

### getActiveNotes() -- Formatted String

Returns a string with all active notes:

```cpp
std::string active = midiHandler.getActiveNotes();
Serial.println(active.c_str());
// Example: "{C4, E4, G4}"
```

### getActiveNotesVector() -- List of Strings

Returns a vector with each note as a string:

```cpp
auto notes = midiHandler.getActiveNotesVector();
// Example: ["C4", "E4", "G4"]

for (const auto& n : notes) {
    Serial.println(n.c_str());
}
```

### getActiveNotesCount() -- Count

Number of notes currently pressed:

```cpp
size_t count = midiHandler.getActiveNotesCount();
Serial.printf("%d active notes\n", (int)count);
```

### fillActiveNotes() -- Boolean Array

Fills a 128-position array (one per MIDI note):

```cpp
bool active[128] = {false};
midiHandler.fillActiveNotes(active);

// Check a specific note:
if (active[60]) Serial.println("C4 is pressed!");

// List all active notes:
for (int i = 0; i < 128; i++) {
    if (active[i]) {
        Serial.printf("Note %d active\n", i);
    }
}
```

---

## Full Example

```cpp
#include <ESP32_Host_MIDI.h>
// Tools > USB Mode -> "USB Host"

void setup() {
    Serial.begin(115200);
    midiHandler.begin();
}

void loop() {
    midiHandler.task();

    // Show active notes when something changes
    static size_t lastCount = 0;
    size_t count = midiHandler.getActiveNotesCount();

    if (count != lastCount) {
        lastCount = count;

        if (count == 0) {
            Serial.println("[ no notes ]");
        } else {
            Serial.printf("[%d notes] %s\n",
                (int)count,
                midiHandler.getActiveNotes().c_str());
        }
    }
}
```

Output when playing a C major chord and releasing:

```
[1 notes] {C4}
[2 notes] {C4, E4}
[3 notes] {C4, E4, G4}
[2 notes] {C4, E4}
[1 notes] {C4}
[ no notes ]
```

---

## Using for Audio Synthesis

`fillActiveNotes()` is ideal for real-time synthesis -- copy the array once per frame:

```cpp
bool notes[128];
midiHandler.fillActiveNotes(notes);

// Synthesize all active notes
for (int i = 0; i < 128; i++) {
    if (notes[i]) {
        float freq = 440.0f * powf(2.0f, (i - 69) / 12.0f);
        // addOscillator(freq);
    }
}
```

---

## Piano Roll with Active Notes

```cpp
#include <ESP32_Host_MIDI.h>

// Show which keys are pressed (ASCII art)
void printPianoRoll() {
    bool notes[128];
    midiHandler.fillActiveNotes(notes);

    // Keys C4 to B4 (60-71)
    for (int i = 60; i <= 71; i++) {
        Serial.print(notes[i] ? "█" : "░");
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

## Clearing Active Notes

In some cases (disconnection, reset) you need to clear the state:

```cpp
// Resets the internal active notes map
midiHandler.clearActiveNotesNow();
```

---

## getActiveNotesString() -- Alternative

Alias for `getActiveNotes()` with an alternative name:

```cpp
std::string s = midiHandler.getActiveNotesString();
// Identical to getActiveNotes()
```

---

## Monitoring Specific Key Presses

Combine `fillActiveNotes()` with a list of notes of interest:

```cpp
const int CHORD_CM[] = {60, 64, 67};  // C, E, G

bool notes[128];
midiHandler.fillActiveNotes(notes);

bool chordComplete = notes[60] && notes[64] && notes[67];
if (chordComplete) {
    Serial.println("C major pressed!");
}
```

---

## Next Steps

- [Chord Detection ->](chord-detection.md) -- group notes into chords
- [GingoAdapter ->](gingo-adapter.md) -- identify chord name
- [PSRAM History ->](psram-history.md) -- store note history
