# esp_master

Firmware da **ESP32-S3 mestre**. Conecta no Wi-Fi (modo station), sobe uma interface
web para montar/enviar rotas e repassa os comandos para a **esp_slave** por UART.

## O que faz
- **Wi-Fi + servidor HTTP** com página de controle embarcada (monta a rota e envia em JSON).
- **Fila de comandos** + **FSM de despacho**: para cada comando, envia ao escravo por UART,
  espera **ACK**, aguarda **DONE** (o escravo executa o movimento medido por encoder) e segue
  para o próximo. **ABORT**/emergência limpam a fila.
- **UART** com o escravo: UART1, **TX=17 / RX=16**, protocolo de 3 bytes `[MSB, LSB, XOR]`.

## Protocolo (mestre → escravo)
Comando 16 bits: `bits[15:14] = estado` (3 = linear/cm, 0 = giro direita/graus, 1 = giro esquerda/graus),
`bits[13:0] = valor`. Sinais do escravo: `0xFFFF` ACK, `0xFFFE` DONE, `0xFFFD` ABORT.

## Endpoints
| Método | URI | Ação |
|---|---|---|
| GET | `/` | página de controle |
| GET | `/api/status` | `{ "fila": N }` |
| POST | `/api/route` | enfileira comandos (`{operacao, rota:[{tipo,valor,direcao}]}`) |
| POST | `/api/emergency` | limpa a fila |

## Configuração
```sh
cp main/secrets.h.example main/secrets.h
```
Edite `main/secrets.h` com seu `WIFI_SSID` e `WIFI_PASS` (não é versionado).

## Compilar e gravar
```sh
cd esp_master
idf.py set-target esp32s3
idf.py build
idf.py -p <PORTA> flash monitor
```

## Ligação UART com o escravo
`MESTRE.TX(17) → ESCRAVO.RX(16)`, `ESCRAVO.TX(17) → MESTRE.RX(16)`, e **GND comum** entre as duas placas.
