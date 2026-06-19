# Roadmap — o que ainda falta

Lista de pendências para evoluir do protótipo atual (esp_slave isolado por BLE) até o
sistema completo previsto no projeto. Contexto detalhado em [contexto-projeto.md](contexto-projeto.md);
histórico em [onde-paramos.md](onde-paramos.md).

## Estado atual (feito)
- `esp_slave` funcional em uma ESP32-S3: controle de motores (BTS7960), sensor de queda
  (IR HW-201) com bloqueio "só ré" na borda, encoders (PCNT) e **controle de linha reta
  em malha fechada** (PI de rumo) que compensa desbalanço e bateria.
- Rota de teste em loop (frente ~1 m + giro 45°, repetindo) para avaliar repetibilidade.
- Controle por BLE (`ROBO_BB8`) + página web (`web/controle_ble.html`).

## Pendências

### 1. Giroscópio (IMU MPU-6050) para melhoria de rota
- Ler a IMU por I²C e usar o yaw para corrigir rumo (sobretudo nas curvas), eliminando o
  pequeno desvio residual que o controle por encoder sozinho deixa.
- Fundir IMU + encoders (odometria + heading) para linha reta e giros de ângulo preciso.

### 2. Comunicação UART entre as duas ESPs (mestre ↔ escravo)
- Implementar o protocolo previsto (3 bytes: `[MSB, LSB, XOR]`).
  - Comando (mestre→escravo): bits `[15:14]` = estado, bits `[13:0]` = valor.
  - Sinais: `0xFFFF` ACK, `0xFFFE` DONE, `0xFFFD` ABORT (escravo→mestre).
- **Deixar o lado do escravo pronto:** task UART que recebe comando, dá ACK, executa e
  responde DONE/ABORT — integrando com o controle de movimento atual.

### 3. ESP mestre
- Wi-Fi + servidor web embarcado (receber rotas em JSON), fila de comandos.
- Conversar com a escrava por UART (despachar comandos, tratar ACK/DONE/ABORT).
- Odometria/telemetria e parada de emergência.

### 4. Máquinas de estado
- **Mestre — FSM de despacho:** IDLE → SEND_COMMAND → WAIT_ACK → MEASUREMENT → SEND_ESTIMATE
  (com reenvio/timeout), conforme o projeto.
- **Mestre — FSM de missão:** Standby → Navegação (vazio) → Acoplamento → Navegação
  (carregado) → Descarga → Marcha ré → (Falha). Ainda não existe em firmware.
- **Escravo — FSM de movimento:** linear / giro / emergência (já parcialmente coberto
  pelo controle atual; alinhar com o protocolo UART).
- Revisar o restante previsto no projeto (ver contexto-projeto.md, seções 4–8).

## Notas de hardware/pinos
- Ver [../docs/ligacoes.md](../docs/ligacoes.md).
- Reservar pinos para I²C (IMU) e UART (mestre↔escravo) — hoje livres.
