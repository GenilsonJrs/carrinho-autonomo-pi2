# Hardware e Pinagem — Carrinho Autônomo (PI2)

Mapa de ligações do projeto: 2× **ESP32‑S3**, drivers de motor, encoders, sensores
IR de queda e giroscópio. Todos os pinos abaixo vêm direto do firmware
(`esp_slave/main/main.c` e `esp_master`), então servem como referência fiel para
visualizar o projeto **ou portar para outra placa**.

---

## 1. Visão geral

```
        ┌──────────────────────────┐         UART (3 fios)        ┌──────────────────────────┐
        │   ESP32‑S3  — MESTRE      │  TX17 ───────────────► RX16 │   ESP32‑S3  — ESCRAVO     │
        │  (cérebro / interface)    │  RX16 ◄─────────────── TX17 │  (atuação + sensores)     │
        │                           │  GND  ───────────────► GND  │                           │
        │  • Wi‑Fi SoftAP           │                             │  • Motores (2× BTS7960)   │
        │    "Carrinho_BB8"         │                             │  • Encoders (2×)          │
        │    http://192.168.4.1/    │                             │  • Sensor de queda (IR)   │
        │  • Servidor web + rotas   │                             │  • Giroscópio MPU‑6050    │
        │  • Fila + FSM de despacho │                             │  • BLE "ROBO_BB8"         │
        └──────────────────────────┘                             └──────────────────────────┘
```

- **Mestre:** recebe rotas/comandos pela página web (cria a própria rede Wi‑Fi) e
  repassa ao escravo por UART. Não tem sensores nem motores.
- **Escravo:** executa o movimento (motores + malha de controle), lê sensores,
  encoders e giroscópio, e também aceita controle direto por **Bluetooth (BLE)**.

---

## 2. ESP32‑S3 ESCRAVO — pinagem completa

> MAC de referência: `14:c1:9f:2a:b7:48`

### 2.1 Motores (drivers BTS7960)

| Função | Driver / Motor | GPIO (ESP32‑S3) |
|---|---|---|
| **M1 — RPWM** | Motor ESQUERDO | **GPIO 4** |
| **M1 — LPWM** | Motor ESQUERDO | **GPIO 5** |
| **M1 — R_EN** | Motor ESQUERDO | **GPIO 7** |
| **M1 — L_EN** | Motor ESQUERDO | **GPIO 15** |
| **M2 — RPWM** | Motor DIREITO | **GPIO 1** |
| **M2 — LPWM** | Motor DIREITO | **GPIO 2** |
| **M2 — R_EN** | Motor DIREITO | **GPIO 41** |
| **M2 — L_EN** | Motor DIREITO | **GPIO 40** |

- PWM: **LEDC**, 20 kHz, resolução 8 bits (duty 0–255).
- `R_EN` e `L_EN` ficam sempre em **nível alto** (driver habilitado); o sentido é
  definido por qual pino PWM recebe duty (RPWM × LPWM).

### 2.2 Encoders (odometria — PCNT)

| Função | GPIO | Observação |
|---|---|---|
| **Encoder ESQUERDO** | **GPIO 10** | 1 canal por roda (contador PCNT) |
| **Encoder DIREITO** | **GPIO 9** | — |

- Alimentação do encoder: **VCC → 3V3**, **GND → GND**.
- ⚠️ Esquerdo/Direito estavam **trocados** no início — manter ESQ=10 e DIR=9, senão
  a malha de rumo dá realimentação positiva.
- Motores **N20 com encoder Hall**. As 4 vias finas do motor são, tipicamente:
  `VCC`, `GND`, `canal A`, `canal B` (as cores variam por fabricante —
  **confirme no seu motor**). Usamos **um canal por roda** ligado ao GPIO acima.

### 2.3 Sensores IR de queda (HW‑201)

| Sensor | Função | GPIO | Lógica |
|---|---|---|---|
| **IR 1** | Sensor de queda (borda) | **GPIO 6** | `0` = chão / `1` = beirada (queda) |
| **IR 2** | Reserva (2º sensor) | **GPIO 8** | mesma lógica |

- Ligação: **VCC → 3V3** (⚠️ não 5V), **GND → GND**, **OUT → GPIO**.
- Potenciômetro azul ajusta a distância de detecção.

### 2.4 Giroscópio MPU‑6050 / GY‑521 (I²C)

| Pino do módulo | GPIO (ESP32‑S3) |
|---|---|
| **SDA** | **GPIO 11** |
| **SCL** | **GPIO 12** |
| **VCC** | **3V3** |
| **GND** | **GND** |
| **AD0** | **GND** (endereço I²C `0x68`) |

- I²C a 400 kHz. Usado só o **eixo Z (yaw)** para os giros precisos.
- Posicionar **plano e firme**, perto do centro de rotação. Manter o carrinho
  **parado ~2 s no boot** (calibração do bias).

### 2.5 UART (link com o mestre)

| Função | GPIO | — |
|---|---|---|
| **TX** (para o mestre) | **GPIO 17** | 115200 8N1 |
| **RX** (do mestre) | **GPIO 16** | — |

### 2.6 Rádio

- **BLE** (NimBLE) — dispositivo `ROBO_BB8`, sem pinos externos.

---

## 3. ESP32‑S3 MESTRE — pinagem

> MAC de referência: `14:c1:9f:2b:2c:e0`

| Função | GPIO | — |
|---|---|---|
| **UART TX** (para o escravo) | **GPIO 17** | 115200 8N1 |
| **UART RX** (do escravo) | **GPIO 16** | — |
| **Wi‑Fi** | interno | SoftAP `Carrinho_BB8` → `http://192.168.4.1/` |

O mestre **não** usa motores, sensores nem encoders — só Wi‑Fi + UART.

---

## 4. Ligação UART entre as duas placas (importante)

As linhas são **cruzadas** e o **GND é comum**:

```
   MESTRE                 ESCRAVO
   GPIO17 (TX) ─────────► GPIO16 (RX)
   GPIO16 (RX) ◄───────── GPIO17 (TX)
   GND        ──────────  GND        (referência comum obrigatória)
```

Protocolo: pacotes de **3 bytes** `[MSB, LSB, XOR]`. Comando = `estado` (2 bits) +
`valor` (14 bits): `3`=frente, `2`=ré (linear, cm); `0`=giro direita, `1`=giro
esquerda (graus). Sinais: `ACK 0xFFFF`, `DONE 0xFFFE`, `ABORT 0xFFFD`,
`STOP 0xFFFC`, manuais `0xFFF0–0xFFF3`.

---

## 5. Driver BTS7960 (cada um — 2 no total)

| Pino do BTS7960 | Liga em |
|---|---|
| `RPWM`, `LPWM` | GPIOs de PWM da ESP (ver tabela 2.1) |
| `R_EN`, `L_EN` | GPIOs de enable da ESP (nível alto = habilitado) |
| `VCC` | 3V3 (lógica) |
| `GND` | GND comum |
| `B+` / `B-` | **Bateria** (potência dos motores) |
| `M+` / `M-` | Terminais do **motor** |

---

## 6. Alimentação

```
   Bateria ──┬──────────────► BTS7960 (B+/B-)   [potência dos motores, direto]
             │
             └──► Step‑down LM2596 ──► 3V3 ──► ESP32‑S3, encoders, sensores IR, MPU‑6050
```

- **Todos os GNDs em comum** (bateria, drivers, ESPs, sensores).
- Sensores, encoders e MPU usam **3V3** neste projeto.
- Mestre e escravo podem ser energizados por fontes/baterias separadas, **desde que
  o GND do link UART seja comum**.

---

## 7. Constantes de calibração (para portar / reajustar)

Definidas em `esp_slave/main/main.c`:

| Constante | Valor | Significado |
|---|---|---|
| `PULSOS_POR_CM` | 74 | pulsos de encoder por cm (reta) — **calibrar por placa/roda** |
| `PULSOS_POR_GRAU` | 9.67 | pulsos por grau (fallback de giro sem gyro) |
| `ROTA_PULSOS_RETA` | 7400 | trecho reto da rota (≈ 1 m) |
| `ROTA_GRAUS_GIRO` | 180 | giro da rota (vai‑e‑volta), feito pelo **yaw do giroscópio** |
| `VEL_RETA` / `VEL_GIRO` / `VEL_MIN` | 200 / 170 / 90 | duty PWM (0–255) |
| `KP_RUMO` / `KI_RUMO` | 0.30 / 0.004 | ganhos do controle de rumo (reta) |
| `GYRO_LSB_POR_DPS` | 131 | escala do MPU‑6050 (±250 °/s) |
| `CTRL_MS` | 50 | período da malha de controle (ms) |

---

## 8. Notas para portar para outra placa

- Qualquer ESP32‑S3 serve; basta manter os **mesmos números de GPIO** ou ajustar os
  `#define` no topo de `esp_slave/main/main.c`.
- Evitar **GPIOs de strapping/flash** (no S3: 0, 3, 19, 20, 26–32, 33–37 em módulos
  com PSRAM/Octal). Os pinos usados aqui já foram escolhidos livres.
- A malha de rumo depende dos **dois encoders**; sem eles, o carrinho ainda anda mas
  não corrige desvio.
- Recalibrar `PULSOS_POR_CM` ao trocar de roda/motor (medir distância real × comandada).
- Os giros usam o **giroscópio** (ângulo), então independem da calibração de pulsos.
