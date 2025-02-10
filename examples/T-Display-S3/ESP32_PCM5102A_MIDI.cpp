#include "ESP32_PCM5102A_MIDI.h"
#include "ESP32_Host_MIDI_Config.h"
#include <driver/i2s.h>
#include <math.h>

// Configurações do I2S
#define I2S_NUM           I2S_NUM_0
#define I2S_SAMPLE_RATE   44100

// Pinos do I2S
#define I2S_BCK_IO        I2S_BCK_PIN
#define I2S_WS_IO         I2S_WS_PIN
#define I2S_DATA_OUT_IO   I2S_DATA_OUT_PIN

ESP32_PCM5102A_MIDI::ESP32_PCM5102A_MIDI()
  : noteActive(false), frequency(0), phase(0), phaseIncrement(0), amplitude(0)
{
}

ESP32_PCM5102A_MIDI::~ESP32_PCM5102A_MIDI() {
  i2s_driver_uninstall(I2S_NUM);
}

void ESP32_PCM5102A_MIDI::begin() {
  // Configuração do I2S exatamente como no exemplo que funcionou
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = I2S_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Estéreo
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 64, // **Exatamente como no código funcional**
      .use_apll = false,
      .tx_desc_auto_clear = true
  };
  
  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCK_IO,
      .ws_io_num = I2S_WS_IO,
      .data_out_num = I2S_DATA_OUT_IO,
      .data_in_num = -1
  };
  
  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
}

void ESP32_PCM5102A_MIDI::playNote(uint8_t note, uint8_t velocity) {
  frequency = midiNoteToFrequency(note);
  
  // **Usando exatamente a mesma amplitude do código funcional**
  amplitude = (int)(velocity / 127.0 * 32767); 
  
  phase = 0;
  phaseIncrement = 2.0 * PI * frequency / sampleRate;
  noteActive = true;
}

void ESP32_PCM5102A_MIDI::stopNote() {
  noteActive = false;
  int16_t zeroBuffer[bufferSize] = {0};
  size_t bytes_written;
  i2s_write(I2S_NUM, zeroBuffer, sizeof(zeroBuffer), &bytes_written, portMAX_DELAY);
}

void ESP32_PCM5102A_MIDI::update() {
  if (!noteActive) return;
  
  int16_t buffer[bufferSize * 2];  // Estéreo
  for (int i = 0; i < bufferSize * 2; i += 2) {
    int16_t sample = (int16_t)(amplitude * sin(phase));
    
    // **Mantendo os dois canais idênticos para som limpo**
    buffer[i] = sample;
    buffer[i + 1] = sample;
    
    phase += phaseIncrement;
    if (phase >= 2.0 * PI) {
      phase -= 2.0 * PI;
    }
  }
  
  size_t bytes_written;
  i2s_write(I2S_NUM, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
}

float ESP32_PCM5102A_MIDI::midiNoteToFrequency(uint8_t note) {
  return 440.0 * pow(2, ((int)note - 69) / 12.0);
}
