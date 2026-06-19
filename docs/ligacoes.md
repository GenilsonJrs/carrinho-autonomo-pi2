# Ligações dos pinos — ESP32-S3 (esp_slave)

Estado atual da fiação do protótipo. Todas as ligações de lógica em **3,3 V** e com
**GND comum** entre ESP, drivers e bateria.

## Visão geral

```
                 +-------------------+
   Bateria ----> |  2x BTS7960 (H)   | ----> Motores N20 (esq/dir) + encoder Hall
                 +-------------------+
                          | sinais (PWM/EN)
                          v
                 +-------------------+      BLE (Web Bluetooth)
   2x IR HW-201  |    ESP32-S3       | <--------------------------- celular/PC
   (queda)  ---> |    (esp_slave)    |
                 +-------------------+
                     ^ encoders (Hall)
```

## Motores — drivers BTS7960 (IBT-2)

| Driver | Sinal | GPIO ESP32-S3 |
|---|---|---|
| Motor 1 (esquerdo) | RPWM | 4 |
| Motor 1 (esquerdo) | LPWM | 5 |
| Motor 1 (esquerdo) | R_EN | 7 |
| Motor 1 (esquerdo) | L_EN | 15 |
| Motor 2 (direito) | RPWM | 1 |
| Motor 2 (direito) | LPWM | 2 |
| Motor 2 (direito) | R_EN | 41 |
| Motor 2 (direito) | L_EN | 40 |
| Ambos | VCC (lógica) | 3V3 |
| Ambos | GND | GND |
| Ambos | R_IS / L_IS | não conectado |

- PWM via LEDC, 20 kHz, 8 bits (0–255).
- Potência (B+/B-) dos drivers vem da bateria; M+/M- vão aos motores.
- Os dois motores estão montados espelhados — o firmware trata isso em `aplicarPWM`.

## Sensores de queda — IR HW-201 (×2)

| Sensor | Pino | GPIO ESP32-S3 |
|---|---|---|
| IR queda (ativo) | OUT | 6 |
| IR reservado | OUT | 8 |
| Ambos | VCC | 3V3 |
| Ambos | GND | GND |

- Lógica: `0` = chão (reflexo) · `1` = queda/borda (sem reflexo).
- Hoje só o da GPIO6 tem função (bloqueia frente/giros, só permite ré). O da GPIO8
  está reservado para uso futuro (detecção de zonas).
- O potenciômetro do módulo ajusta a distância de detecção.

## Encoders — Hall dos motores N20

| Fio | Função | GPIO ESP32-S3 |
|---|---|---|
| Preto | GND | GND |
| Azul | VCC | 3V3 |
| Amarelo (motor pwmEsq) | sinal A | 10 |
| Amarelo (motor pwmDir) | sinal A | 9 |
| Verde | sinal B | não conectado |

- Leitura por PCNT (contagem de pulsos em hardware), 1 canal por roda.
- **Atenção:** na bancada os encoders estavam cruzados em relação aos motores;
  os GPIOs 9/10 foram atribuídos para casar cada encoder com seu motor (feedback
  negativo correto na malha de linha reta). Se trocar de placa/fiação, revalidar.
- Usados para o controle de linha reta (igualar velocidades + manter rumo) e para
  medir distância/ângulo da rota em loop.

## Pinos livres reservados para o futuro

- **IMU MPU-6050 (I²C):** SDA/SCL a definir (ex.: GPIO 11/12 ou 35/36 conforme módulo).
- **UART para a esp_master:** TX/RX a definir (o projeto previa UART1/UART2 em 16/17).

## Observações de gravação (USB-Serial-JTAG)

- A porta COM muda a cada reset/reconexão — redetectar pelo VID `303A` antes de gravar.
- Se o auto-reset de flash falhar: power-cycle, ou modo download manual
  (segurar BOOT, apertar RST, soltar BOOT).
