# 🔍 Troubleshooting

Soluções para os problemas mais comuns ao usar ESP32_Host_MIDI.

---

## USB Host

### ❌ Teclado USB não é detectado

**Sintomas:** Serial Monitor não mostra nada ao pressionar teclas.

**Verificações:**

1. **USB Mode correto?**
   ```
   Arduino IDE → Tools → USB Mode → "USB Host"
   ```
   Esta opção só aparece para ESP32-S3, S2 ou P4.

2. **Placa correta selecionada?**
   Apenas ESP32-S3, S2 e P4 têm USB-OTG. ESP32 Classic não suporta.

3. **Cabo OTG correto?**
   - Use um cabo **USB-OTG host** (micro-A ou C para USB-A fêmea)
   - Não use um cabo de dados normal — a pinagem é diferente
   - Verifique se o cabo suporta dados (não apenas carga)

4. **Teclado é class-compliant?**
   Teste conectando ao macOS — se reconhecer sem driver, é class-compliant.

5. **Alimentação suficiente?**
   ```
   Serial.printf("Tensão USB: %.2fV\n", /* medir na linha VBUS */);
   ```
   Dispositivos USB MIDI precisam de pelo menos 100 mA a 5V.

---

### ❌ "USB Mode" não aparece no menu Tools

**Causa:** Placa selecionada não suporta USB-OTG.

**Solução:** Mude para "LilyGo T-Display-S3", "ESP32-S3 Dev Module", ou outra placa S3/S2/P4.

---

### ❌ Upload falha após selecionar "USB Host"

**Causa:** O modo USB Host muda o comportamento do USB CDC.

**Soluções:**

1. Pressione o botão **BOOT** + **RST** para entrar em modo bootloader
2. Ou use o modo de upload via UART (não OTG): conecte via adaptador USB-UART
3. Após o upload, desconecte/reconecte ou pressione RST

---

## BLE MIDI

### ❌ Dispositivo BLE não aparece no iOS/macOS

**Sintomas:** O nome configurado não aparece na lista de dispositivos Bluetooth.

**Verificações:**

1. **Bluetooth habilitado no sdkconfig?**
   ```
   Arduino IDE → Tools → Partition Scheme → "Default 4MB with spiffs"
   ```
   Certifique-se de usar uma partition scheme que inclua BLE (>1.5 MB de app).

2. **Chip suporta BLE?**
   ESP32-S2 e ESP32-P4 **não têm Bluetooth**. Use ESP32, S3, C3, ou C6.

3. **Verificar a macro:**
   ```cpp
   #if ESP32_HOST_MIDI_HAS_BLE
       Serial.println("BLE disponível");
   #else
       Serial.println("BLE NÃO disponível neste chip");
   #endif
   ```

4. **Advertising iniciado?**
   ```cpp
   midiHandler.begin();
   // O BLE começa a anunciar automaticamente após begin()
   delay(1000);
   Serial.println("BLE advertising...");
   ```

5. **Conflito de UUID?**
   Se outro dispositivo BLE com o mesmo nome estiver próximo, pode conflitar.
   Mude `cfg.bleName` para um nome único.

---

### ❌ BLE desconecta frequentemente

**Causas possíveis:**

- Distância > 15 m (paredes reduzem alcance)
- Interferência de outros dispositivos 2,4 GHz (WiFi, microondas)
- Alimentação insuficiente (queda de tensão no pico BLE)

**Soluções:**

- Adicionar capacitor 100µF na alimentação do ESP32
- Reduzir potência do WiFi se estiver usando WiFi + BLE simultaneamente
- O BLE reinicia o advertising automaticamente — sem ação necessária no código

---

## RTP-MIDI (WiFi)

### ❌ ESP32 não aparece em Audio MIDI Setup

**Verificações:**

1. **ESP32 e Mac na mesma rede WiFi?**
   ```cpp
   Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
   ```
   Confirme que o IP é da mesma sub-rede (ex: 192.168.1.x / 192.168.1.x).

2. **Firewall do macOS bloqueando mDNS?**
   Verifique em: System Settings → Network → Firewall → Allow incoming connections.

3. **AppleMIDI-Library v3.x instalada?**
   ```
   Manage Libraries → "AppleMIDI" → versão ≥ 3.0.0
   ```

4. **WiFi conectado ANTES de rtpMIDI.begin()?**
   ```cpp
   WiFi.begin(ssid, pass);
   while (WiFi.status() != WL_CONNECTED) delay(500);  // ← Esperar!
   rtpMIDI.begin("Meu ESP32");  // ← Só depois
   ```

5. **mDNS ativo?**
   A biblioteca AppleMIDI usa MDNS. Verifique se seu router não bloqueia mDNS (multicast).

---

### ❌ RTP-MIDI conecta mas sem som no DAW

**Causa:** A sessão de rede precisa ser habilitada manualmente no Audio MIDI Setup.

**Solução:**
1. Audio MIDI Setup → Network → selecione a sessão → clique **Connect**
2. No DAW: verifique se a porta MIDI "Meu ESP32" está habilitada como input

---

## UART / DIN-5

### ❌ Nenhuma mensagem recebida via DIN-5

**Verificações:**

1. **Pinos RX/TX corretos?**
   ```cpp
   uartMIDI.begin(Serial1, /*RX=*/16, /*TX=*/17);
   // Confirme que o cabo MIDI está em MIDI IN do instrumento
   // e conectado ao pino RX do ESP32
   ```

2. **Optoacoplador corretamente cabeado?**
   Teste sem opto: conecte o pino de saída do opto diretamente a 3.3V via resistor 1kΩ.
   Se o RX detectar HIGH, o pino está funcionando.

3. **Baud rate correto?**
   MIDI usa **31250 bps** — não confundir com 31200, 38400 ou outros valores similares.

4. **Instrumento enviando MIDI?**
   Teste com outro dispositivo (computador + MIDI interface) para confirmar que o instrumento envia MIDI.

5. **GPIO 0 não está sendo usado como RX?**
   GPIO 0 é o botão de boot — evite usar para UART.

---

## PSRAM

### ❌ PSRAM não é detectada / enableHistory() falha

**Verificações:**

1. **PSRAM habilitada no Arduino IDE?**
   ```
   Tools → PSRAM → "OPI PSRAM" (T-Display-S3)
                ou "Quad PSRAM" (outros S3)
   ```

2. **Verificar disponibilidade:**
   ```cpp
   Serial.printf("PSRAM: %u bytes\n", ESP.getPsramSize());
   if (ESP.getPsramSize() == 0) {
       Serial.println("PSRAM não detectada!");
   }
   ```

3. **Macro correta:**
   ```cpp
   #if ESP32_HOST_MIDI_HAS_PSRAM
       Serial.println("PSRAM disponível");
   #else
       Serial.println("PSRAM NÃO disponível");
   #endif
   ```

---

## Compilação

### ❌ "error: 'map' was not declared"

**Causa:** `#include <map>` faltando em algum header.

**Solução:** Este bug foi corrigido em versões recentes. Atualize a biblioteca:
```
Manage Libraries → ESP32_Host_MIDI → Update
```

---

### ❌ "addTransport() limit exceeded"

**Causa:** Você registrou mais de 4 transportes externos via `addTransport()`.

**Solução:** O limite é 4 transportes externos. USB, BLE e ESP-NOW built-in não contam.
Combine transportes ou reduza o número de transportes externos.

---

## Debug — Callback Raw MIDI

Para inspecionar bytes brutos antes do parsing:

```cpp
void onRaw(const uint8_t* raw, size_t len, const uint8_t* midi3) {
    Serial.printf("RAW [%d bytes]: ", (int)len);
    for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", raw[i]);
    }
    Serial.printf("| MIDI: %02X %02X %02X\n",
        midi3[0], midi3[1], midi3[2]);
}

void setup() {
    midiHandler.setRawMidiCallback(onRaw);
    midiHandler.begin();
}
```

---

## Abrir uma Issue

Se o problema persiste após todas as verificações:

1. Anote a versão da biblioteca (`library.properties`)
2. Anote o chip e a placa
3. Copie o Serial Monitor (com debug raw callback)
4. Abra uma issue em [github.com/sauloverissimo/ESP32_Host_MIDI/issues](https://github.com/sauloverissimo/ESP32_Host_MIDI/issues)

---

## Próximos Passos

- [Hardware Suportado →](hardware.md) — verificar compatibilidade do chip
- [API →](../api/referencia.md) — conferir assinaturas corretas
- [Primeiros Passos →](../guia/primeiros-passos.md) — voltar ao básico
