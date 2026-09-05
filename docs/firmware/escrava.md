# Firmware da escrava

`esp_slave/` · C sobre **ESP-IDF v5.5** + FreeRTOS

É a placa que move o robô. Recebe comandos da mestre por UART e executa cada movimento em malha
fechada, respondendo com `ACK`, `DONE` ou `ABORT`.

## Recursos

| Recurso | Implementação |
|---|---|
| **Acionamento dos motores** | PWM via **LEDC a 20 kHz**, drivers BTS7960 |
| **Odometria** | **Encoders Hall** com contagem direta em hardware pelo periférico **PCNT** |
| **Rumo** | Giroscópio **MPU-6050** por I²C, estimando *yaw* |
| **Controle** | Malha fechada **PI** de rumo |
| **Segurança** | Sensor **IR reflexivo** de queda |
| **Link com a mestre** | **UART**, 3 bytes com checksum XOR |
| **Controle manual** | **BLE**, dispositivo `ROBO_BB8` |

## O controle de rumo

O ponto mais interessante do firmware, e o que separa um robô que anda de um robô que anda **reto**.

**Na linha reta**, a malha PI compensa duas fontes de desvio que aparecem na prática:

- **Desbalanço entre os motores** — dois motores nominalmente iguais não giram igual
- **Queda de tensão da bateria** — conforme a carga cai, a resposta dos motores muda

**Nos giros**, o fechamento é feito **por ângulo do giroscópio**, não por pulso de encoder.

!!! tip "Por que o giroscópio nos giros"
    Girar contando pulsos de encoder acumula erro: derrapagem da roda, folga mecânica e diferença
    de diâmetro entre as rodas fazem o ângulo real divergir do calculado. Cada giro herda o erro
    dos anteriores, e ao fim de uma rota o robô está longe do lugar.

    Fechar o giro pelo ângulo medido corta esse acúmulo — cada giro se corrige contra a medida
    real, não contra a estimativa.

Há **fallback por encoder** e **timeout**: se o giroscópio falhar ou o giro não completar, o
movimento termina de forma controlada em vez de travar a missão.

## Sensor de queda

Ao detectar borda, o firmware:

1. **Aborta** o movimento em curso
2. **Para** os motores
3. Envia **`ABORT`** à mestre, que limpa a fila

É proteção de hardware com consequência de software: o robô não só para, como comunica, e a
missão inteira é cancelada em vez de continuar com o robô fora de posição.

## Comandos BLE

Característica `0xFF01`, dispositivo `ROBO_BB8`.

| Comando | Ação |
|:---:|---|
| `F` | Frente |
| `B` | Ré |
| `L` | Girar à esquerda |
| `R` | Girar à direita |
| `A` | Inicia a rota de teste em *loop* |
| `S` | Parar (aborta o movimento) |
| `1` | Calibração: reta de 1 metro |
| `9` | Calibração: giro de 90° à direita |
| `8` | Calibração: giro de 90° à esquerda |

Os três últimos existem para **calibrar as constantes** de conversão entre pulsos, graus e
distância real — sem eles, portar o firmware para outro chassi seria tentativa e erro.

## Compilar

```bash
cd esp_slave
idf.py set-target esp32s3
idf.py build
idf.py -p <PORTA> flash monitor
```

Ver [compilar e gravar](../compilar.md) e a [pinagem](../hardware.md).
