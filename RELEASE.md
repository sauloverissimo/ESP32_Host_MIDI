```markdown
# 🚀 ESP32_Host_MIDI - Release 2.0 🎹📡

Welcome to version **2.0** of the **ESP32_Host_MIDI** library! This update brings significant improvements over the previous version, including performance optimizations, new features, and enhanced stability for MIDI communication via **USB and BLE** on the **ESP32-S3** with **T-Display S3**.

---

## 🔥 Key Updates and Improvements

### 🏗️ Enhanced Modular Architecture
- Improved code structure for easier maintenance and expansion.
- Clear separation between **USB MIDI**, **BLE MIDI**, **Display Output**, and **MIDI Processing** modules.

### 🎛️ New MIDI Processing Queue
- The **MIDI_Handler** now uses a **dynamic queue** (`std::deque<MIDIEventData>`) to store MIDI events, providing more flexibility and better control over data flow.
- Added an **adjustable event limit** to prevent overloading.

### 📡 Improved USB MIDI Host
- **Reworked USB MIDI packet processing** ensures that only the **first 4 relevant bytes** are used, reducing latency.
- Optimized **USB message queue**, increasing efficiency in handling received MIDI packets.
- **New callback system** for USB MIDI device connection and disconnection events.

### 📲 Enhanced BLE MIDI Support
- The **ESP32 now functions as a stable and reliable BLE MIDI server**.
- **Direct callback for processing received BLE MIDI messages**.
- Optimized BLE advertising for better compatibility with mobile MIDI apps.

### 🎨 Improved MIDI Display Output
- Uses **LovyanGFX** for optimized rendering on the **ST7789**.
- New `printMessage()` method in **ST7789_Handler**:
  - Displays **multi-line messages**.
  - **Automatically highlights** the last line for better separation of MIDI events.
  - Reduces flickering by avoiding unnecessary screen refreshes.

### 🏎️ Performance Boost
- **Better use of USB interrupts**, reducing the load on the main loop.
- **Removal of unnecessary delays**, improving real-time MIDI event processing.
- **Optimized chord and sequence detection**.

---

## 📂 Code Structure in Version 2.0

The new file structure is designed for modularity and clarity:

📁 **/src/**
- `USB_Conexion.h / USB_Conexion.cpp` → Manages USB MIDI communication.
- `BLE_Conexion.h / BLE_Conexion.cpp` → Manages BLE MIDI communication.
- `MIDI_Handler.h / MIDI_Handler.cpp` → Interprets and organizes MIDI events.
- `ESP32_Pin_Config.h` → Defines ESP32 pin configurations.

📁 **/examples/**
- Complete example for **T-Display S3**, showcasing **MIDI USB + BLE + Display** integration.

---

## ⚡ How to Upgrade to Version 2.0?

1. **Download the updated library files** and replace the old ones.
2. **Check your code**:
   - If you were using `onMidiMessage()` directly, now you should override `onMidiDataReceived()` in **USB_Conexion** or **BLE_Conexion**.
   - To display MIDI messages on **ST7789**, use `printMessage()` in **ST7789_Handler**.

---

## 🛠️ How to Use the New Version?

### 1️⃣ Initialize USB MIDI Communication:
```cpp
USB_Conexion usbMIDI;
usbMIDI.begin();
```

### 2️⃣ Initialize BLE MIDI Communication:
```cpp
BLE_Conexion bleMIDI;
bleMIDI.begin();
```

### 3️⃣ Process MIDI in the Main Loop:
```cpp
void loop() {
    usbMIDI.task();
    bleMIDI.task();
}
```

### 4️⃣ Display MIDI Messages on the Screen:
```cpp
ST7789_Handler display;
display.init();
display.printMessage("MIDI Active!", 0, 0);
```

---

## 🏆 Conclusion

This update **significantly improves the stability, efficiency, and functionality of ESP32_Host_MIDI**. If you were using the previous version, we highly recommend upgrading to take full advantage of all the new features and optimizations!

Thanks to the community for the feedback and suggestions! 🎶💙

---
📌 **ESP32_Host_MIDI 2.0 - Faster, More Stable, More Complete!** 🚀🎵


```markdown
# 🚀 ESP32_Host_MIDI - Versão 2.0 🎹📡

Bem-vindo à versão **2.0** da biblioteca **ESP32_Host_MIDI**! Esta atualização traz melhorias significativas em relação à versão anterior, incluindo otimizações de desempenho, novos recursos e maior estabilidade para comunicação MIDI via **USB e BLE** no **ESP32-S3** com **T-Display S3**.

---

## 🔥 Principais Atualizações e Melhorias

### 🏗️ Arquitetura Modular Aprimorada
- Estrutura de código melhorada para facilitar a manutenção e expansão.
- Separação clara entre os módulos de **USB MIDI**, **BLE MIDI**, **Saída no Display** e **Processamento MIDI**.

### 🎛️ Nova Fila de Processamento MIDI
- O **MIDI_Handler** agora usa uma **fila dinâmica** (`std::deque<MIDIEventData>`) para armazenar eventos MIDI, proporcionando mais flexibilidade e melhor controle sobre o fluxo de dados.
- Adicionado um **limite ajustável de eventos** para evitar sobrecarga.

### 📡 Melhorias no Host USB MIDI
- **Reformulação do processamento de pacotes USB MIDI**, garantindo que apenas os **4 primeiros bytes relevantes** sejam utilizados, reduzindo a latência.
- Otimização da **fila de mensagens USB**, aumentando a eficiência no tratamento dos pacotes MIDI recebidos.
- **Novo sistema de callbacks** para eventos de conexão e desconexão de dispositivos MIDI USB.

### 📲 Suporte Aprimorado para BLE MIDI
- O **ESP32 agora funciona como um servidor MIDI BLE mais estável e confiável**.
- **Callback direto para processar mensagens MIDI recebidas via BLE**.
- Publicidade BLE otimizada para maior compatibilidade com aplicativos MIDI móveis.

### 🎨 Melhorias na Exibição MIDI no Display
- Utiliza **LovyanGFX** para otimização da renderização no **ST7789**.
- Novo método `printMessage()` no **ST7789_Handler**:
  - Exibe **mensagens em várias linhas**.
  - **Destaque automático** para a última linha, melhorando a separação dos eventos MIDI.
  - Redução de flickering ao evitar atualizações desnecessárias da tela.

### 🏎️ Aumento de Desempenho
- **Melhor uso de interrupções USB**, reduzindo a carga no loop principal.
- **Remoção de delays desnecessários**, melhorando o processamento em tempo real dos eventos MIDI.
- **Otimização na detecção de acordes e sequências de notas**.

---

## 📂 Estrutura do Código na Versão 2.0

A nova estrutura de arquivos foi projetada para modularidade e clareza:

📁 **/src/**
- `USB_Conexion.h / USB_Conexion.cpp` → Gerencia a comunicação USB MIDI.
- `BLE_Conexion.h / BLE_Conexion.cpp` → Gerencia a comunicação BLE MIDI.
- `MIDI_Handler.h / MIDI_Handler.cpp` → Interpreta e organiza eventos MIDI.
- `ESP32_Pin_Config.h` → Define as configurações de pinos do ESP32.

📁 **/examples/**
- Exemplo completo para **T-Display S3**, demonstrando a integração **MIDI USB + BLE + Display**.

---

## ⚡ Como Atualizar para a Versão 2.0?

1. **Baixe os arquivos atualizados da biblioteca** e substitua os arquivos antigos.
2. **Verifique seu código**:
   - Se você estava usando `onMidiMessage()` diretamente, agora deve sobrescrever `onMidiDataReceived()` em **USB_Conexion** ou **BLE_Conexion**.
   - Para exibir mensagens MIDI no **ST7789**, use `printMessage()` no **ST7789_Handler**.

---

## 🛠️ Como Usar a Nova Versão?

### 1️⃣ Inicializar Comunicação USB MIDI:
```cpp
USB_Conexion usbMIDI;
usbMIDI.begin();
```

### 2️⃣ Inicializar Comunicação BLE MIDI:
```cpp
BLE_Conexion bleMIDI;
bleMIDI.begin();
```

### 3️⃣ Processar MIDI no Loop Principal:
```cpp
void loop() {
    usbMIDI.task();
    bleMIDI.task();
}
```

### 4️⃣ Exibir Mensagens MIDI na Tela:
```cpp
ST7789_Handler display;
display.init();
display.printMessage("MIDI Ativo!", 0, 0);
```

---

## 🏆 Conclusão

Esta atualização **melhora significativamente a estabilidade, eficiência e funcionalidade do ESP32_Host_MIDI**. Se você estava usando a versão anterior, recomendamos fortemente a atualização para aproveitar todos os novos recursos e otimizações!

Agradecemos à comunidade pelo feedback e sugestões! 🎶💙

---
📌 **ESP32_Host_MIDI 2.0 - Mais Rápido, Mais Estável, Mais Completo!** 🚀🎵
```

