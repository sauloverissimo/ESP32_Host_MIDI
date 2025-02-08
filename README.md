# ESP32_Host_MIDI 🎹📡

Projeto desenvolvido para ARDUINO IDE:

Este projeto oferece uma solução completa para receber, interpretar e exibir mensagens MIDI via USB no ESP32 (especialmente ESP32-S3) com o T‑Display S3.

---

## Português 🇧🇷

### Visão Geral
A biblioteca **ESP32_Host_MIDI** permite que o ESP32 atue como host USB para dispositivos MIDI, interprete os dados recebidos (utilizando funções do módulo **MIDI_Handler**) e exiba essas informações no T‑Display S3 através do **DisplayHandler**. A biblioteca é modular, facilitando adaptações para outros hardwares, bastando ajustar os arquivos de configuração.

### Estrutura dos Arquivos
- **ESP32_Host_MIDI_Config.h**  
  Define os pinos usados para comunicação USB e para o display.  
  - *Exemplo:* `USB_DP_PIN`, `USB_DN_PIN`, `TFT_CS_PIN`, `TFT_DC_PIN`, `TFT_RST_PIN`, `TFT_BL_PIN`.

- **ESP32_Host_MIDI.h / ESP32_Host_MIDI.cpp**  
  Gerencia a comunicação USB MIDI.  
  - **Funções Principais:**  
    - `begin()`: Inicializa o USB Host e registra o cliente.  
    - `task()`: Processa os eventos USB e submete as transferências.  
    - `onMidiMessage(const uint8_t *data, size_t length)`: Função virtual chamada quando uma mensagem MIDI é recebida. Deve ser sobrescrita para tratar a mensagem (por exemplo, utilizando o **MIDI_Handler**).

- **MIDI_Handler.h / MIDI_Handler.cpp**  
  Fornece funções estáticas que interpretam os dados MIDI brutos (após remover o cabeçalho USB) em diversos formatos:  
  - **Raw Format:** Ex.: `[0x09, 0x90, 0x3C, 0x64]`  
  - **Short Format:** Ex.: `"90 3C 64"`  
  - **Note Number:** Ex.: `"60"`  
  - **Tipo de Mensagem:** Ex.: `"NoteOn"`, `"NoteOff"`, `"Control Change"`, `"Program Change"`, etc.  
  - **Status:** Ex.: `"9n"`  
  - **Note Sound com Octave:** Ex.: `"C5"`  
  - **Message Vector:** Estrutura que reúne os campos interpretados.

- **datahandler.h**  
  Define tipos de dados para manipulação estruturada dos dados (ex.: `TypeElement`, `TypeVector`, `TypeTable`, `TypeCube`).  
  Esses tipos auxiliam na organização e manipulação dos dados MIDI interpretados.

- **displayhandler.h / displayhandler.cpp**  
  Gerencia a exibição das informações no T‑Display S3 utilizando a biblioteca LovyanGFX.  
  - **Funções Principais:**  
    - `init()`: Inicializa o display, configura a rotação (180°), dimensões (320x170) e tamanho da fonte.  
    - `printMidiMessage(const char* message)`: Exibe a mensagem formatada em 5 linhas, com a última ("Octave") destacada (linha separadora, fonte maior e cor diferenciada).  
    - `clear()`: Limpa o display.
  
- **ESP32_Host_MIDI.ino**  
  Exemplo de implementação no T‑Display S3. Integra a recepção USB MIDI, a interpretação dos dados (via **MIDI_Handler**) e a exibição das informações no display.  
  - Utiliza uma classe derivada que sobrescreve `onMidiMessage()` para:
    - Remover o cabeçalho USB dos dados.
    - Formatar os dados em 5 linhas (Raw, Short, Note#, Msg e Octave).
    - Exibir a mensagem e limpar o display após 1 segundo.

### Funcionamento
1. **Recepção MIDI:**  
   Ao conectar um dispositivo MIDI, o ESP32 captura os dados USB.  
2. **Interpretação:**  
   A função virtual `onMidiMessage()` é chamada. Uma classe derivada remove o cabeçalho (primeiro byte) e utiliza o **MIDI_Handler** para converter os bytes em formatos legíveis.
3. **Exibição:**  
   O **DisplayHandler** exibe os dados formatados em 5 linhas (com destaque na última linha) no T‑Display S3, que está rotacionado 180° para a orientação correta.

---

## English 🇺🇸 🗺️

### Overview
**ESP32_Host_MIDI** is a library that enables the ESP32 (especially the ESP32-S3) to function as a USB host for MIDI devices, interpret incoming MIDI data (using functions from the **MIDI_Handler** module), and display the formatted information on a T‑Display S3 via the **DisplayHandler**. The library is modular and easily configurable for other hardware platforms.

### File Structure
- **ESP32_Host_MIDI_Config.h**  
  Defines the pin configuration for USB communication and the display.  
  - *Examples:* `USB_DP_PIN`, `USB_DN_PIN`, `TFT_CS_PIN`, `TFT_DC_PIN`, `TFT_RST_PIN`, `TFT_BL_PIN`.

- **ESP32_Host_MIDI.h / ESP32_Host_MIDI.cpp**  
  Manages USB MIDI communication.  
  - **Key Functions:**  
    - `begin()`: Initializes the USB host and registers the client.  
    - `task()`: Processes USB events and submits transfers.  
    - `onMidiMessage(const uint8_t *data, size_t length)`: A virtual function triggered when a MIDI message is received; it should be overridden to handle the message (e.g., using **MIDI_Handler**).

- **MIDI_Handler.h / MIDI_Handler.cpp**  
  Provides static functions to interpret raw MIDI messages (after removing the USB header) into various readable formats:  
  - **Raw Format:** e.g., `[0x09, 0x90, 0x3C, 0x64]`  
  - **Short Format:** e.g., `"90 3C 64"`  
  - **Note Number:** e.g., `"60"`  
  - **Message Type:** e.g., `"NoteOn"`, `"NoteOff"`, `"Control Change"`, `"Program Change"`, etc.  
  - **Status:** e.g., `"9n"`  
  - **Note Sound with Octave:** e.g., `"C5"`  
  - **Message Vector:** A structured vector containing all interpreted fields.

- **datahandler.h**  
  Defines data types for structured data manipulation (e.g., `TypeElement`, `TypeVector`, `TypeTable`, `TypeCube`).  
  These types are used internally by **MIDI_Handler** to organize the interpreted MIDI data.

- **displayhandler.h / displayhandler.cpp**  
  Manages the display on the T‑Display S3 using LovyanGFX.  
  - **Key Functions:**  
    - `init()`: Initializes the display, setting the rotation (180°), dimensions (320x170), and font size.  
    - `printMidiMessage(const char* message)`: Displays the formatted message across 5 lines, with the last line ("Octave") highlighted (separator line, larger font, and distinct color).  
    - `clear()`: Clears the display.

- **ESP32_Host_MIDI.ino**  
  Example implementation for the T‑Display S3. Integrates USB MIDI reception, data interpretation via **MIDI_Handler**, and display output.  
  - A derived class overrides `onMidiMessage()` to:
    - Remove the USB header from the data.
    - Format the data into 5 lines (Raw, Short, Note#, Msg, and Octave).
    - Display the message and clear the screen after 1 second.

### How It Works
1. **MIDI Reception:**  
   When a MIDI device is connected, the ESP32 receives USB MIDI data.
2. **Data Interpretation:**  
   The virtual function `onMidiMessage()` is triggered. A derived class removes the header (first byte) and uses **MIDI_Handler** functions to convert the bytes into readable formats.
3. **Display Output:**  
   The **DisplayHandler** displays the formatted message across 5 lines (with the last line highlighted) on the T‑Display S3, which is rotated 180° for proper orientation.

---

## Español 🇪🇸 🩻

### Descripción General
**ESP32_Host_MIDI** es una biblioteca diseñada para que el ESP32 (especialmente el ESP32-S3) funcione como host USB para dispositivos MIDI, interprete los datos MIDI recibidos (utilizando funciones del módulo **MIDI_Handler**) y muestre la información formateada en un T‑Display S3 a través del **DisplayHandler**. La biblioteca es modular y se puede configurar fácilmente para otros dispositivos.

### Estructura de Archivos
- **ESP32_Host_MIDI_Config.h**  
  Define la configuración de pines para la comunicación USB y la pantalla.  
  - *Ejemplos:* `USB_DP_PIN`, `USB_DN_PIN`, `TFT_CS_PIN`, `TFT_DC_PIN`, `TFT_RST_PIN`, `TFT_BL_PIN`.

- **ESP32_Host_MIDI.h / ESP32_Host_MIDI.cpp**  
  Gestiona la comunicación USB MIDI.  
  - **Funciones Clave:**  
    - `begin()`: Inicializa el host USB y registra el cliente.  
    - `task()`: Procesa los eventos USB y envía las transferencias.  
    - `onMidiMessage(const uint8_t *data, size_t length)`: Función virtual que se llama cuando se recibe un mensaje MIDI; debe sobrescribirse para procesar el mensaje (por ejemplo, usando **MIDI_Handler**).

- **MIDI_Handler.h / MIDI_Handler.cpp**  
  Proporciona funciones estáticas para interpretar los mensajes MIDI crudos (después de eliminar el encabezado USB) en varios formatos legibles:  
  - **Formato Raw:** ej.: `[0x09, 0x90, 0x3C, 0x64]`  
  - **Formato Short:** ej.: `"90 3C 64"`  
  - **Número de Nota:** ej.: `"60"`  
  - **Tipo de Mensaje:** ej.: `"NoteOn"`, `"NoteOff"`, `"Control Change"`, `"Program Change"`, etc.  
  - **Status:** ej.: `"9n"`  
  - **Nota con Octava:** ej.: `"C5"`  
  - **Vector de Mensaje:** Una estructura que agrupa todos los campos interpretados.

- **datahandler.h**  
  Define tipos de datos para la manipulación estructurada (por ejemplo, `TypeElement`, `TypeVector`, `TypeTable`, `TypeCube`).  
  Estos tipos se usan internamente en **MIDI_Handler** para organizar los datos MIDI interpretados.

- **displayhandler.h / displayhandler.cpp**  
  Gestiona la visualización en el T‑Display S3 utilizando LovyanGFX.  
  - **Funciones Clave:**  
    - `init()`: Inicializa la pantalla, estableciendo la rotación (180°), dimensiones (320x170) y el tamaño de la fuente.  
    - `printMidiMessage(const char* message)`: Muestra el mensaje formateado en 5 líneas, destacando la última línea ("Octave") con una línea separadora, fuente más grande y color diferente.  
    - `clear()`: Limpia la pantalla.

- **ESP32_Host_MIDI.ino**  
  Ejemplo de implementación para el T‑Display S3. Integra la recepción USB MIDI, la interpretación de datos mediante **MIDI_Handler** y la salida en pantalla.  
  - Una clase derivada sobrescribe `onMidiMessage()` para:
    - Eliminar el encabezado USB de los datos.
    - Formatear los datos en 5 líneas (Raw, Short, Note#, Msg y Octave).
    - Mostrar el mensaje y limpiar la pantalla después de 1 segundo.

### Cómo Funciona
1. **Recepción MIDI:**  
   Al conectar un dispositivo MIDI, el ESP32 recibe los datos MIDI vía USB.
2. **Interpretación de Datos:**  
   Se invoca la función virtual `onMidiMessage()`. Una clase derivada elimina el encabezado (primer byte) y utiliza las funciones de **MIDI_Handler** para convertir los bytes en formatos legibles.
3. **Salida en Pantalla:**  
   El **DisplayHandler** muestra el mensaje formateado en 5 líneas (con la última línea destacada) en el T‑Display S3, que se rota 180° para la orientación correcta.

---

## 🌟 Notas y Estilo Visual

- **Emojis:** 🎹, 📡 y otros para representar visualmente la funcionalidad MIDI.  
- **Colores Destacados:**  
  - La última línea ("Octave") se muestra en **rojo** para destacarse.  
- **Formato del Display:**  
  - Pantalla en modo horizontal (320x170) con 180° de rotación.  
  - Texto distribuido en 5 líneas, separando la última línea con una línea horizontal.

---

*Divirta-se explorando e desenvolvendo seus projetos com ESP32_Host_MIDI!* 🚀  
*Enjoy exploring and developing your projects with ESP32_Host_MIDI!* 🚀  
*¡Disfruta explorando y desarrollando tus proyectos con ESP32_Host_MIDI!* 🚀
