# Onde paramos — diário de progresso

> Log curto de sessões de trabalho na integração do carrinho. O mais recente fica no topo.
> Contexto geral do projeto: ver [contexto-projeto.md](contexto-projeto.md).

---

## 2026-06-18 — Teste do sensor de queda IR (HW-201) ✅ CONCLUÍDO

**Objetivo (atingido):** testar isolado o sensor IR de queda numa ESP32-S3 e obter o código dele.
Projeto standalone em [../esp_test_sensor/](../esp_test_sensor/) (`main/main.c` lê GPIO e loga estado;
`capture_serial.py` captura a serial; `diag_read.py` foi diagnóstico de DTR/RTS).

### Sensor: HW-201 (IR de obstáculo/reflexão)
- 3 pinos: **VCC→3V3** (NÃO 5V), **GND→GND**, **OUT→GPIO6** (mesmo pino do `esp_slave`).
- Potenciômetro azul = ajuste da distância de detecção. Tem LED de power e LED de output.

### Resultado validado (48 transições capturadas)
- **Lógica confirmada, igual ao `esp_slave`:** `nivel=0` = CHÃO (há reflexo) | `nivel=1` = BURACO/QUEDA (sem reflexo).
  Bate com `lerDistanciaSolo()` (nível 1 ⇒ emergência).
- **Chattering perto do limiar** (~100 ms): nesses módulos LM393 é normal quando a superfície fica na borda
  da distância ajustada. **AÇÃO p/ firmware final:** colocar *debounce* (exigir ~2–3 leituras consecutivas /
  ~60 ms antes de aceitar a mudança), sobretudo no `0→1` (queda), senão um falso "1" curto dispara emergência.
- Potenciômetro deve ser regulado na altura real de uso.
- **Os DOIS módulos IR do projeto foram testados na bancada com o mesmo firmware e funcionam igual**
  (um tem LED indicador vermelho, outro verde — diferença só cosmética). São os dois sensores previstos:
  o de **queda** e o de **detecção de zonas no piso**. Bancada concluída; falta integrar no firmware.

### ⚠️ Gotchas de gravação/serial nesta placa (IMPORTANTE)
- A porta COM **muda a cada reconexão/reset**: já foi COM17 → COM14 → COM20. **Sempre redetectar antes de gravar/capturar:**
  ```powershell
  Get-CimInstance Win32_PnPEntity | ? { $_.DeviceID -match "VID_303A" -and $_.Name -match "COM" } | Select Name
  ```
- O **auto-reset de gravação às vezes trava** (USB-Serial-JTAG). Soluções (na ordem): **(a) desligar/ligar a
  placa (power-cycle)** restaura o auto-reset — funcionou aqui; **(b) modo download manual** — segurar BOOT,
  apertar/soltar RST, soltar BOOT; depois `idf.py -B C:\bb8_sensor_build -p COMxx flash`.
- **LED RGB onboard (WS2812):** acende em cor aleatoria ao energizar. O firmware do sensor agora o apaga no
  boot via RMT (envia 0,0,0 no GPIO48 e 38) — ver `ws2812_apagar()` em `esp_test_sensor/main/main.c`.
- **Captura da serial** (logs via USB-Serial-JTAG, pois usei `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`):
  só funciona com **DTR=True / RTS=False** (senão 0 bytes). Ver `esp_test_sensor/capture_serial.py`.
  Build do sensor em `C:\bb8_sensor_build`.

### Próximo passo
Integrar o sensor de queda (com debounce) no firmware do escravo, e depois o 2º sensor IR (zonas).
Antes disso, ainda pendente do dia anterior: ver o carrinho andando reto no chão.

---

## 2026-06-17 — Bring-up dos motores (BLE, 1 ESP32-S3) ✅ CONCLUÍDO

**Objetivo do dia (atingido):** fazer os motores girarem usando UMA ESP32-S3 + drivers
BTS7960, controlados por BLE, só para validar a fiação. Não envolveu o split mestre/escravo.

### O que foi criado
Projeto **standalone** de teste em [../esp_test_motor/](../esp_test_motor/) (NÃO mexe em `esp_master`/`esp_slave`):
- `main/main.c` — servidor GATT BLE (NimBLE) + controle de motores via LEDC.
- `main/CMakeLists.txt`, `CMakeLists.txt`, `sdkconfig.defaults` (habilita `CONFIG_BT_ENABLED` + `CONFIG_BT_NIMBLE_ENABLED`).
- `controle_ble.html` — página Web Bluetooth de controle (versão limpa do HTML que o Genilson enviou).

### Como compilar/gravar (IMPORTANTE p/ próxima vez)
- ESP-IDF **v5.5.1** em `C:\Users\genil\esp\v5.5.1\esp-idf`. `idf.py` NÃO está no PATH — tem que ativar:
  ```powershell
  . C:\Users\genil\esp\v5.5.1\esp-idf\export.ps1
  ```
- Build em diretório **fora do OneDrive** (evita caminho longo + lock de sync): `C:\bb8_build`.
  ```powershell
  cd c:\Users\genil\OneDrive\Documentos\GitHub\projeto_grupo08\esp_test_motor
  idf.py -B C:\bb8_build set-target esp32s3   # só na 1a vez
  idf.py -B C:\bb8_build build
  idf.py -B C:\bb8_build -p COM17 flash
  ```
- **Placa:** ESP32-S3 (QFN56, rev v0.2, PSRAM 8MB) na **COM17** (USB-Serial/JTAG). MAC `3c:dc:75:5c:76:64`.
  As outras COM (3/4/7/8) são Bluetooth do Windows, ignorar.

### Controle BLE
- Device **`ROBO_BB8`**, serviço **`0x00FF`**, característica **`0xFF01`** (write). 1 byte por comando.
- Comandos: `F` frente, `B` ré, `L` gira esq, `R` gira dir, `S` parar, `A` frente (teste).
- Abrir `controle_ble.html` no **Chrome/Edge** (Web Bluetooth; não funciona Firefox/Safari). Segurar = move, soltar = para.

### Pinos (iguais ao esp_slave, p/ reaproveitar fiação)
- Motor 1 **esquerdo**: R_PWM=GPIO4, L_PWM=GPIO5, REN=GPIO7, LEN=GPIO15 → LEDC ch 0/1.
- Motor 2 **direito**: R_PWM=GPIO1, L_PWM=GPIO2, REN=GPIO41, LEN=GPIO40 → LEDC ch 2/3.
- PWM LEDC 20 kHz, 8 bits. Velocidades de teste: `VEL_RETA=200`, `VEL_GIRO=170` (de 255).

### Calibração aplicada (após testes na bancada)
Os dois motores estão **montados espelhados**. Estado final do `aplicarPWM` (já validado pelo Genilson):
- Esquerdo: **frente → L_PWM** (CH_M1_L); ré → R_PWM.
- Direito:  **frente → R_PWM** (CH_M2_R); ré → L_PWM.
- Em `handle_command`, os comandos **L e R foram trocados** (estavam invertidos na 1ª versão).
- Resultado confirmado: **frente, ré e os dois giros corretos.** Teste de motores OK.

> Histórico do ajuste (caso precise reverter o raciocínio): v1 ambos simétricos (R_PWM=frente) →
> direito ia ao contrário. v2 inverti só o direito → os dois passaram a ir pra trás no "frente".
> v3 inverti ambos (forma espelhada acima) → frente OK. v4 troquei L↔R → giros OK.

### Próximos passos (para amanhã)
1. Colocar o carrinho **no chão** e testar deslocamento reto; se puxar para um lado, ajustar o
   **balanço de velocidade** entre os lados (compensar diferença de motor/atrito).
2. Depois: migrar para a **arquitetura final mestre↔escravo** (UART), ou portar o controle de motor
   validado para o `esp_slave`. Lembrar que no projeto real o motor/IR fica no escravo e a odometria no mestre.
