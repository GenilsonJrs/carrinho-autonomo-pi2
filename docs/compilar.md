# Compilar e gravar

**Pré-requisito:** ESP-IDF v5.5 ou superior.

## Escrava

```bash
cd esp_slave
idf.py set-target esp32s3
idf.py build
idf.py -p <PORTA> flash monitor
```

## Mestre

Configure o Wi-Fi **antes** de compilar:

```bash
cd esp_master
cp main/secrets.h.example main/secrets.h   # edite WIFI_SSID / WIFI_PASS
idf.py set-target esp32s3
idf.py build
idf.py -p <PORTA> flash monitor
```

## Ligação UART entre as placas

```
MESTRE.TX (17)  ──────►  ESCRAVO.RX (16)
ESCRAVO.TX (17) ──────►  MESTRE.RX  (16)
GND ─────────────────── GND  (comum)
```

!!! danger "O GND comum não é opcional"
    Sem referência de terra compartilhada, os níveis lógicos não têm significado entre as placas.
    A comunicação passa a falhar de forma **intermitente** — o pior tipo de defeito, porque
    funciona o suficiente para enganar e falha o suficiente para atrapalhar.

## Gravação via USB-Serial-JTAG

O ESP32-S3 grava pela porta USB nativa. Se a placa não for detectada, o caminho usual é entrar em
modo de gravação manualmente: segurar **BOOT**, pulsar **RESET**, soltar **BOOT**.

Detalhes e os pinos reservados estão em [ligações da escrava](ligacoes.md).

## Depois de gravar

1. Ligue as duas placas e confirme o **GND comum**
2. Conecte-se ao Wi-Fi da mestre (ou à rede em que ela entrou)
3. Abra o **IP da mestre** no navegador
4. Monte uma rota curta e teste antes de carregar um trajeto completo

!!! tip "Calibre antes da primeira missão"
    Os comandos BLE `1`, `9` e `8` executam reta de 1 metro e giros de 90° em cada sentido.
    Rodá-los e conferir o resultado real com trena e transferidor é o que ajusta as constantes de
    conversão para o seu chassi — rodas de diâmetro diferente ou folga mecânica mudam os números.

    As constantes estão documentadas na [pinagem](hardware.md), na seção de calibração.
