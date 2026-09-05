# Interface e rotas

## Duas formas de controlar

=== "Via mestre (Wi-Fi)"

    Acesse o **IP da ESP mestre** no navegador. É a interface completa:

    - Montar a rota visualmente
    - **Simular no canvas** antes de executar
    - Importar e exportar a rota em JSON
    - Controle manual por *D-pad*
    - Telemetria
    - **Parada de emergência**

=== "Standalone (BLE)"

    Abra [`web/controle_ble.html`](https://github.com/GenilsonJrs/carrinho-autonomo-pi2/blob/main/web/controle_ble.html)
    no **Chrome ou Edge**, clique em *Conectar Bluetooth* e selecione `ROBO_BB8`.

    Fala direto com a escrava, sem passar pela mestre. Útil para testar movimento e calibrar sem
    depender de rede.

!!! note "Por que Chrome ou Edge"
    A interface standalone usa **Web Bluetooth**, API que o Firefox e o Safari não implementam.

## Formato da rota

Cada ação tem quatro campos:

| Campo | Valores |
|---|---|
| `tipo` | `mover` · `girar` |
| `valor` | número |
| `unidade` | centímetros ou graus |
| `direcao` | `forward` · `clockwise` · `anticlockwise` |

## Rotas da apresentação final

As duas rotas efetivamente usadas na apresentação estão versionadas no repositório, no mesmo
formato que a interface importa:

| Arquivo | Ações | Descrição |
|---|:---:|---|
| [`rota-1-apresentacao.json`](https://github.com/GenilsonJrs/carrinho-autonomo-pi2/blob/main/rotas/rota-1-apresentacao.json) | 6 | Reta longa, curva de 90° e aproximação ao ponto de entrega |
| [`rota-2-apresentacao.json`](https://github.com/GenilsonJrs/carrinho-autonomo-pi2/blob/main/rotas/rota-2-apresentacao.json) | 9 | Múltiplas curvas (109°, 22°, 93°) e retas encadeadas |

**Para reproduzir:** abra a interface do mestre, clique em **Carregar JSON** e selecione um dos
arquivos.

!!! tip "Ângulos quebrados são o teste real"
    A rota 2 usa 109°, 22° e 93° — não múltiplos de 90°. São justamente esses valores que provam
    o controle por giroscópio: fechar um giro de 22° por contagem de pulso de encoder tem margem
    de erro alta demais para uma trajetória encadeada.

## Próximos passos

- Evoluir o controle proporcional/PI para **PID completo**, com acelerações mais suaves
- **Sensor TOF frontal** para desvio de obstáculos — hoje o IR só detecta queda
- **Geração automática de rotas** na interface, por exemplo com planejamento A*
