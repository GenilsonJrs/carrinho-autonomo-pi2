# Contexto do Projeto — Grupo 08 (PI2 / UnB Gama)

> **Para que serve este arquivo:** documento de contextualização criado para servir de referência rápida
> durante o trabalho de integração do carrinho autônomo. Resume o **que é o projeto**, **como o sistema está
> arquitetado**, **o que está realmente implementado no firmware** e **onde estão os pontos de atenção para a
> integração**. Sempre que precisar de contexto, leia este arquivo primeiro.
>
> Última atualização do mapeamento: 2026-06-16.

---

## 1. O que é o projeto

**Veículo Autônomo de Transporte de Carga ("BB8")** — robô móvel terrestre desenvolvido na disciplina
**Projeto Integrador 2 (PI2)** da **UnB Gama (FGA)**, Grupo 08, semestre 2026/01.

- **Missão:** transportar de forma 100% autônoma um *pallet* com três cargas (~5 kg no total) entre três
  pontos demarcados no piso: **Partida → Carregamento → Chegada**, sem intervenção humana durante a operação.
- **Princípio de projeto central:** **simplicidade mecânica**. O carregamento/descarregamento é **passivo**
  (garfos fixos tipo empilhadeira encaixam no pallet pelo próprio avanço; a marcha ré + estrutura fixa de
  descarga separam o pallet). **Não há servos nem atuadores no engate.**
- **Restrições de projeto:** robô ≤ 50×50×50 cm; autonomia mínima 30 min; operação em ambiente interno
  controlado.
- **Repositório (GitLab):**
  `https://gitlab.com/unb-esw/fga-pi2/semestre-2026-01/grupo-8/projeto_grupo08.git`
- **Marcos:** PC1 (Concepção, 13/04/2026 ✔), PC2 (Construção e Testes, 15/05/2026 ✔), **PC3 (Integração e
  Produto Final, 25/06/2026 — em andamento).** A integração que vamos fazer é justamente o esforço de PC3.

### Disciplinas/áreas envolvidas
Software, Eletrônica/Energia e Estruturas (Aeroespacial). Liderança de Software: **Gabriel Monteiro**;
Eletrônica: **Luiz Guilherme Aguiar** (autor de boa parte do firmware); Gerência: **Mateus Santana**.
O usuário (**Genilson Junior**) faz parte da **equipe de Software**.

---

## 2. Arquitetura física do sistema (blocos)

Orquestrado por **ESP32‑S3** (modelo de referência ESP32‑S3‑DevKit‑N8R8). Cinco blocos funcionais:

| Bloco | Componentes | Função |
|---|---|---|
| **Alimentação** | Bateria → Drivers BTS7960 (direto) + Regulador step‑down LM2596 (5V/3.3V) | 5V p/ IMU; 3.3V p/ ESP32‑S3, encoders, IR |
| **Entrada** | Sensores IR (zonas), IMU MPU‑6050 (I²C), Encoders Hall (N20, 11 ppr), Sensor de borda/queda | Percepção e odometria |
| **Saída** | 2× Driver BTS7960 (ponte H), 2× Motor N20 c/ redução e encoder | Tração diferencial |
| **Comunicação externa** | Wi‑Fi 802.11 b/g/n (servidor HTTP embarcado). *Bluetooth/BLE foi removido em 18/05/2026.* | Interface web de controle/telemetria |
| **Atuação mecânica passiva** | Garfos de acoplamento, pallet, estrutura fixa de descarga | Carga/descarga sem eletrônica |

> **Atenção:** o documento `docs/.../arquitetura-sistema.md` descreve **um único ESP32‑S3** fazendo tudo.
> Essa é a **concepção original**. O **firmware realmente implementado divide o sistema em DOIS ESP32‑S3**
> (mestre + escravo) comunicando por UART — ver seção 4. Essa diferença é o ponto mais importante para a
> integração.

---

## 3. Estrutura do repositório

```
projeto_grupo08/
├── .ia/                      # (este diretório) notas de contexto p/ IA
├── esp_master/               # FIRMWARE MESTRE (ESP32-S3) — Wi-Fi + web + fila + odometria + FSM
│   ├── CMakeLists.txt
│   ├── README.md             # guia de build/flash do mestre (bom material)
│   └── main/
│       ├── main.c            # bootstrap + loop FSM a cada 20 ms
│       ├── fsm.{c,h}         # máquina de estados de despacho de comandos
│       ├── queue.{c,h}       # fila circular de comandos (mutex), comando em 16 bits
│       ├── encoder.{c,h}     # odometria via PCNT (Pulse Counter)
│       ├── uart_comm.{c,h}   # comunicação UART com o escravo
│       ├── wifi_sta.{c,h}    # Wi-Fi modo station
│       ├── web_server.{c,h}  # esp_http_server + endpoints REST
│       ├── uart_protocol.{c,h}  # LEGADO: NÃO está no CMakeLists (não compila); .h vazio
│       └── interface/        # index.html, app.js, style.css (embutidos via EMBED_FILES)
├── esp_slave/
│   └── 1-FW_Navegação_P/     # FIRMWARE ESCRAVO (ESP32-S3) — controle de motores + segurança
│       ├── CMakeLists.txt
│       └── main/
│           ├── main.cpp      # tasks FreeRTOS: navegação (PID-P) + leitura IR
│           └── uart_com.{c,h}# UART + decodificação de comandos do mestre
├── docs/                     # documentação MkDocs (site do projeto)
│   ├── index.md, Relatorio-final*.md
│   ├── Geral/                # TAP, EAP, requisitos, cronograma, riscos, normas...
│   ├── PontoDeControle_01/   # docs de concepção (Software, Estruturas, Eletrônica)
│   └── PontoDeControle_02/   # docs de construção/testes (Firmware, Software, Hardware, Energia)
├── build/                    # artefatos de build (ignorável)
├── mkdocs.yml? / README.md   # README raiz + config do site
└── CONTRIBUTING.md, CODE_OF_CONDUCT.md
```

---

## 4. Arquitetura de firmware — **MESTRE × ESCRAVO** (o coração da integração)

São **dois firmwares independentes em dois ESP32‑S3**, ligados por **UART** (3 fios: TX, RX, GND).
A divisão de responsabilidades é incomum e precisa ser bem entendida:

```
        [Navegador / celular]
                │  HTTP/Wi-Fi (REST JSON)
                ▼
   ┌─────────────────────────────┐        UART 115200 8N1 (3 bytes)       ┌─────────────────────────────┐
   │       ESP_MASTER (S3)        │  ── comando (estado+setpoint) ──▶      │       ESP_SLAVE (S3)        │
   │  • Wi-Fi STA + servidor HTTP │  ◀── ACK / DONE / ABORT ──             │  • Controle PID-P dos motores│
   │  • Fila de comandos (queue)  │  ── estimativa (LOCALIZACAO) ──▶       │  • PWM LEDC 20kHz p/ ponte H │
   │  • Odometria (ENCODER/PCNT)  │                                        │  • Sensor IR (queda) + emerg.│
   │  • FSM de despacho           │                                        │  • Não lê encoder próprio    │
   └─────────────────────────────┘                                        └─────────────────────────────┘
```

### Insight arquitetural mais importante
A **malha de controle é distribuída entre os dois MCUs**:
- O **encoder físico está ligado ao MESTRE** (PCNT). O mestre calcula distância/ângulo percorridos.
- O mestre **envia a posição estimada (`LOCALIZACAO`) ao escravo** via UART.
- O **escravo** usa `erro = SETPOINT − LOCALIZACAO` no seu controlador **Proporcional (P)** para acionar os
  motores, e devolve `DONE` quando atinge o alvo.
- Ou seja: **mestre = "cérebro" (odometria + fila + web)**, **escravo = "atuador" (motores + segurança)**.

### 4.1 MESTRE (`esp_master`)
- `app_main` (em `main/main.c`): inicializa encoder → fila → Wi‑Fi+HTTP → UART → FSM, e roda
  `fsm_update()` em loop **a cada 20 ms** (`vTaskDelayUntil`).
- **FSM** (`fsm.c`), `MAX_RETRIES = 5`:
  1. `IDLE` — espera comando na fila.
  2. `SEND_COMMAND` — codifica `estado_bits<<14 | (valor & 0x3FFF)`, faz `uart_flush_rx()` e envia 3 bytes.
  3. `WAIT_ACK` — espera `0xFFFF` por ~10 ms; sem ACK reenvia até 5×; estourou → **limpa a fila** e volta a IDLE.
  4. `MEASUREMENT` — `encoder_update_position()` (distância p/ linear, ângulo p/ angular).
  5. `SEND_ESTIMATE` — envia a posição ao escravo; lê sinais: `DONE`→próximo comando; `ABORT`→limpa fila;
     senão volta a `MEASUREMENT` (continua mandando estimativa). Cede o tick a cada 20 ms.
- **Encoder** (`encoder.c`): usa **um único canal PCNT** (`ENCR_PIN`). Constantes em `encoder.h`:
  `ESP_VARIANT 2` (S3), `ENCR_PIN=10`, `ENCL_PIN=48`, 11 pulsos/rev, redução **169**, raio roda **4.0 cm**,
  separação rodas **15.0 cm**. Calcula `DISTANCE_PER_PULSE` e `ANGLE_PER_PULSE`.
- **Fila** (`queue.{c,h}`): circular, `MAX_COMMAND_QUEUE=1024`, `MAX_COMMAND_VALUE=0x3FFF` (14 bits),
  protegida por mutex. Tipos: `CMD_TYPE_LINEAR=0`, `CMD_TYPE_ANGULAR=1`. Direções:
  `CMD_DIR_FORWARD_CLOCKWISE=0`, `CMD_DIR_BACKWARD_COUNTERCLOCKWISE=1`.
- **Wi‑Fi** (`wifi_sta.c`): modo STA. As credenciais ficam em `main.c`
  (`WIFI_SSID`/`WIFI_PASSWORD`) e **não devem ser versionadas** — definir conforme a rede de teste.

### 4.2 ESCRAVO (`esp_slave/1-FW_Navegação_P`)
- `app_main` (em `main/main.cpp`): inicializa IR + ponte H + UART e cria 2 tasks FreeRTOS:
  - `task_uart` (Core 0, prioridade 6) — recebe/decodifica pacotes.
  - `task_navegacao` (Core 1, prioridade 5) — FSM de movimento + controle P, roda a cada 20 ms.
- **Estados de navegação** (`EstadoRobo`): `estado_1` (linear), `estado_2` (giro direita),
  `estado_3` (giro esquerda), `estado_4_EMERGENCIA`.
- **Controle P:** `Kp_linear=5.0` (satura ±255), `Kp_angular=3.0` (satura ±150).
  Tolerâncias de parada: **1.0 cm** (linear) e **2.0°** (angular). Ao atingir → `enviarSinalConclusaoMestre()` (DONE).
- **PWM:** periférico **LEDC**, 20 kHz, 8 bits (0–255), `LEDC_LOW_SPEED_MODE`, canais 0–3.
- **Pinos dos motores** (em `main.cpp`):
  - Motor 1 (esquerdo): R_PWM=GPIO4, L_PWM=GPIO5, REN=GPIO7, LEN=GPIO15.
  - Motor 2 (direito): R_PWM=GPIO1, L_PWM=GPIO2, REN=GPIO41, LEN=GPIO40.
- **Sensor IR de queda:** GPIO6, polling a cada 20 ms. `0`=chão seguro, `1`=buraco.
  Emergência: para → recua a −150 por 2 s → envia `ABORT` → volta a `estado_2`.

---

## 5. Protocolo UART mestre↔escravo

- **Físico:** 115200 bps, 8 dados, sem paridade, 1 stop (8N1), sem controle de fluxo.
- **Pinos:** mestre **UART_NUM_1** TX=GPIO17 / RX=GPIO16; escravo **UART_NUM_2** TX=GPIO17 / RX=GPIO16.
  → Ligação **cruzada**: `MESTRE.TX(17) → ESCRAVO.RX(16)` e `ESCRAVO.TX(17) → MESTRE.RX(16)` + GND comum.
- **Pacote = 3 bytes:** `[MSB, LSB, CHECKSUM]`, onde `CHECKSUM = MSB ^ LSB`. Se o XOR não bater, o byte é
  descartado e o buffer é realinhado (ambos os lados implementam esse realinhamento).

### 5.1 Codificação do comando (mestre → escravo)
Payload 16 bits: **bits [15:14] = código de estado**, **bits [13:0] = valor (setpoint)**.

| Comando | `estado_bits` enviado | Escravo mapeia para |
|---|---|---|
| Linear (mover) | **3** | `estado_1` (linear) |
| Girar à esquerda (CCW / `anticlockwise`/`backward`) | **1** | `estado_3` (giro esquerda) |
| Girar à direita (CW / `clockwise`/`forward`) | **0** | `estado_2` (giro direita) |

> Obs.: a direção de **mover** (frente/ré) não é transmitida em bit separado — o mestre só distingue linear
> (3) vs. giro (0/1). O valor é o módulo em **cm** (linear) ou **graus** (giro).

### 5.2 Sinais reservados (16 bits)
| Hex | Nome | Significado | Quem envia |
|---|---|---|---|
| `0xFFFF` | `SINAL_ACK` | comando recebido OK | escravo → mestre |
| `0xFFFE` | `SINAL_DONE` | objetivo concluído (chegou no setpoint) | escravo → mestre |
| `0xFFFD` | `SINAL_ABORT` | emergência (sensor de queda) — **JÁ IMPLEMENTADO** | escravo → mestre |

### 5.3 Estimativa (mestre → escravo)
Depois do ACK, o mestre envia periodicamente a posição medida pelo encoder. O escravo, com
`aguardando_localizacao=true`, interpreta o pacote como `LOCALIZACAO = dado & 0x7FFF`.

---

## 6. Interface web + API REST (no mestre)

Página HTML/CSS/JS **embutida no firmware** via `EMBED_FILES` (sem SD/SPIFFS). Permite montar uma **rota**
(sequência de trechos com ações mover/girar), visualizar em canvas, e enviar como JSON.

| Método | URI | Descrição |
|---|---|---|
| GET | `/`, `/index.html` | página |
| GET | `/app.js`, `/style.css` | assets |
| GET | `/api/telemetry` | `{distance_cm, angle_degrees, queue_size}` (reais) |
| POST | `/api/route` | enfileira comandos (`operacao`: `append`/`restart`) |
| POST | `/api/emergency` | limpa a fila imediatamente |

**Exemplo de corpo `POST /api/route`:**
```json
{
  "operacao": "append",
  "rota": [
    { "trecho": "Trecho 1", "acoes": [
      { "tipo": "mover", "valor": 100, "unidade": "cm",    "direcao": "forward" },
      { "tipo": "girar", "valor": 90,  "unidade": "graus", "direcao": "clockwise" }
    ]}
  ]
}
```
- `tipo`: `mover`→`CMD_TYPE_LINEAR`; `girar`→`CMD_TYPE_ANGULAR`.
- `direcao`: `forward`/`clockwise`→bit 0; `backward`/`anticlockwise`→bit 1.
- `valor`: 0–16383 (14 bits).

---

## 7. Stack tecnológico

- **Firmware:** C (mestre) e C++ (escravo) sobre **ESP‑IDF v5.5+** (dev em v5.5.4) + **FreeRTOS**.
- **Hardware:** 2× **ESP32‑S3**; motores **N20** c/ encoder Hall; drivers **BTS7960**; IMU **MPU‑6050**;
  sensores IR; regulador **LM2596**.
- **Periféricos ESP‑IDF usados:** PCNT (encoder), UART, LEDC (PWM), GPIO, esp_http_server, esp_wifi, cJSON.
- **Web:** HTML/CSS/JavaScript puro (sem framework), `fetch` para REST.
- **Documentação:** **MkDocs** (Material) — site servido de `docs/`.

### Build & flash (resumo — ver `esp_master/README.md`)
```powershell
# em um shell com ambiente ESP-IDF ativo:
idf.py set-target esp32s3      # AMBAS as placas são ESP32-S3
idf.py build
idf.py -p COM<x> flash monitor # Ctrl+] sai do monitor
```
Antes de compilar o mestre: ajustar `WIFI_SSID`/`WIFI_PASSWORD` em `main/main.c` e conferir
`ESP_VARIANT` em `encoder.h` (=2 para S3). O escravo usa GPIOs 40/41/48 → confirma que também é S3.

Rodar o site de docs localmente:
```powershell
py -m pip install mkdocs mkdocs-material pymdown-extensions mkdocs-git-revision-date-localized-plugin
python -m mkdocs serve   # http://127.0.0.1:8000/
```

---

## 8. Pontos de atenção para a INTEGRAÇÃO ⚠️

Divergências entre documentação e código, e lacunas que provavelmente entram no escopo de PC3:

1. **Mono‑MCU (docs) × dois MCUs (código):** `arquitetura-sistema.md` fala em um único ESP32‑S3; o firmware
   real é **mestre + escravo via UART**. A integração precisa das **duas placas** ligadas (TX/RX cruzados + GND).
2. **Odometria mora no MESTRE, motores no ESCRAVO.** O encoder físico precisa estar ligado ao **mestre**
   (PCNT em GPIO10/48). O escravo **não lê encoder próprio** — depende do `LOCALIZACAO` enviado pelo mestre.
   Se o encoder for fisicamente cabeado no escravo, a malha não fecha.
3. **Apenas 1 canal de encoder** é lido (`ENCR_PIN`), e o mesmo contador vira distância **ou** ângulo conforme
   o tipo de comando. **Não há fusão com IMU** nem segundo encoder no firmware, apesar de a doc citar
   MPU‑6050 + dois encoders. Integração de IMU = trabalho em aberto.
4. **Mestre não aciona motor nem lê IR** — isso é só do escravo. O texto do `esp_master/README.md` que diz
   "o loop consome um comando a cada segundo" está **desatualizado** (o loop real é a FSM a 20 ms; o
   acionamento de motor vive no escravo).
5. **Telemetria parcialmente mockada:** bateria, yaw da IMU e encoders por roda são **simulados** em `app.js`
   (`atualizarMocksRestantes`). Reais: `distance_cm`, `angle_degrees`, `queue_size`. Para "des‑mockar",
   expor os valores em `web_server.c:telemetry_handler` e remover o badge MOCK no `index.html`.
6. **`SINAL_ABORT` já está implementado** no escravo (emergência IR) e tratado no mestre
   (`UART_RX_ABORT` → limpa fila). A doc `Arquitetura_de_Navegação_e_Controle.md` §4.3 ainda diz
   "reservado para implementação futura" — está **desatualizada**.
7. **`uart_protocol.{c,h}` são legado:** `uart_protocol.h` está vazio e `uart_protocol.c` **não está no
   `main/CMakeLists.txt`** (não é compilado). A UART real está em `uart_comm.{c,h}`. Não confiar nesses arquivos.
8. **Direção de "mover" (frente/ré) não trafega** no protocolo atual (só linear=3 vs giro=0/1). Se a missão
   exigir marcha ré comandada pela rota (ela exige, no desacoplamento), isso é uma lacuna do protocolo a resolver.
9. **Máquina de estados da MISSÃO** (Standby→Navegação→Acoplamento→…→Falha, descrita em
   `arquitetura-sistema.md`) **não existe no firmware** — hoje há apenas a FSM de despacho (mestre) e a FSM de
   movimento (escravo). A lógica de missão completa (detectar zonas por IR, acoplar, descarregar) ainda
   não está codificada.

---

## 9. Documentos‑chave para consultar (em `docs/`)

| Assunto | Arquivo |
|---|---|
| Termo de abertura / escopo / requisitos | `docs/Geral/tap.md`, `docs/Geral/Levantamento_de_requisitos.md` |
| Arquitetura de sistema (blocos, estados, REST) | `docs/PontoDeControle_01/Software/arquitetura-sistema.md` |
| Arquitetura de navegação/controle (escravo, baixo nível) | `docs/PontoDeControle_02/Firmware/Arquitetura_de_Navegação_e_Controle.md` |
| FSM do mestre | `docs/PontoDeControle_02/Firmware/esp_master_fsm.md` |
| UART mestre / escravo | `docs/PontoDeControle_02/Firmware/esp_master_uart.md`, `esp_slave_uart.md` |
| Integração interface ↔ mestre (com vídeo) | `docs/PontoDeControle_02/esp_master-interface.md` |
| Interface de controle / testes | `docs/PontoDeControle_02/Software/interface-controle.md`, `testes-interface.md` |
| Esquemático / BOM / protocolos HW | `docs/PontoDeControle_02/Hardware/` |
| Guia de build/flash do mestre | `esp_master/README.md` |

---

## 10. Glossário rápido de identificadores

- `command_queue_t` — fila circular de comandos no mestre.
- `fsm_state_t` (mestre): `IDLE`, `SEND_COMMAND`, `WAIT_ACK`, `MEASUREMENT`, `SEND_ESTIMATE`.
- `EstadoRobo` (escravo): `estado_1` (linear), `estado_2` (giro dir), `estado_3` (giro esq), `estado_4_EMERGENCIA`.
- `ESTADO`, `SETPOINT`, `LOCALIZACAO` — globais `volatile` compartilhadas entre tasks no escravo.
- `encoder_position_t` — `{distance_cm, angle_degrees, mutex}` no mestre.
- Sinais UART: `SINAL_ACK=0xFFFF`, `SINAL_DONE=0xFFFE`, `SINAL_ABORT=0xFFFD`.
