# Arquitetura

## Divisão de responsabilidades

O sistema usa **duas ESP32-S3** com papéis distintos. Essa separação não é enfeite: isola o que
tem prazo apertado do que pode esperar.

```
        Navegador (interface web embarcada)
                    │  HTTP (monta e envia a rota em JSON)
                    ▼
        ┌──────────────────────────┐        UART1 · 3 bytes [MSB, LSB, XOR]
        │      ESP32-S3 MESTRE     │  ◄───────────────────────────────────┐
        │  Wi-Fi (SoftAP/STA)      │   ACK / DONE / ABORT                 │
        │  Servidor HTTP + SPA     │                                      │
        │  Fila de comandos + FSM  │  ── comando (estado + setpoint) ──►  │
        └──────────────────────────┘                                      │
                                                                          ▼
                                            ┌──────────────────────────────┐
                                            │      ESP32-S3 ESCRAVA        │
                                            │  Motores BTS7960 (PWM/LEDC)  │
                                            │  Encoders (PCNT)             │
                                            │  Giroscópio MPU-6050 (rumo)  │
                                            │  Sensor IR de queda          │
                                            │  Controle de rumo em malha   │
                                            │  fechada + BLE (ROBO_BB8)    │
                                            └──────────────────────────────┘
```

| Placa | Responsabilidade |
|---|---|
| **Mestre** | Falar com o mundo: Wi-Fi, servidor HTTP, interface web, fila de comandos e despacho |
| **Escrava** | Mover o robô: motores, encoders, giroscópio, sensor de queda e controle em malha fechada |

!!! tip "Por que separar"
    O controle de rumo em malha fechada tem exigência de tempo real: atraso na leitura do
    giroscópio vira desvio de trajetória. Deixar a pilha Wi-Fi e o servidor HTTP na **mesma**
    placa competiria por CPU justamente com o laço que mantém o robô na reta.

    Com a divisão, a escrava só faz uma coisa — e faz no tempo certo.

## O protocolo entre as placas

Comando de **16 bits**, transmitido em 3 bytes: `[MSB, LSB, XOR]`.

```
bits[15:14] = estado
bits[13:0]  = valor
```

| Estado | Significado | Valor |
|:---:|---|---|
| `3` | Linear | distância em centímetros |
| `0` | Giro à direita | ângulo em graus |
| `1` | Giro à esquerda | ângulo em graus |

**Sinais de resposta da escrava:**

| Código | Significado |
|---|---|
| `0xFFFF` | **ACK** — comando recebido |
| `0xFFFE` | **DONE** — movimento concluído |
| `0xFFFD` | **ABORT** — movimento interrompido (queda detectada ou emergência) |

O terceiro byte é o **XOR** dos dois primeiros. Em um link serial entre placas, ruído elétrico dos
motores é uma fonte real de corrupção — o checksum evita que um bit trocado vire um comando de
movimento errado.

## O ciclo de uma missão

1. O operador monta a rota na interface web e envia
2. A mestre **enfileira** os comandos
3. Para cada comando: envia por UART, espera **ACK**, aguarda **DONE**
4. Só então despacha o próximo
5. Um **ABORT** ou parada de emergência **limpa a fila** inteira

!!! warning "Uma ação por vez, sempre"
    A mestre não envia o próximo comando antes do `DONE` do anterior. É o que garante que a rota
    execute na ordem certa, sem sobreposição de movimentos — e o que faz o `ABORT` conseguir
    interromper a missão de forma limpa.

## Ligação física entre as placas

```
MESTRE.TX (17)  ──────►  ESCRAVO.RX (16)
ESCRAVO.TX (17) ──────►  MESTRE.RX  (16)
GND ─────────────────── GND  (comum, obrigatório)
```

O **GND comum** não é opcional: sem referência de terra compartilhada, os níveis lógicos não têm
significado entre as placas e a comunicação falha de forma intermitente — o tipo de defeito que
consome horas de depuração.

Detalhes completos em [pinagem](hardware.md).
