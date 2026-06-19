# esp_master

Pasta reservada para o firmware da **ESP32-S3 mestre**.

Ainda não implementado. Responsabilidades previstas:

- Servir a interface web (Wi-Fi) e receber as rotas.
- Manter a fila de comandos e a máquina de estados da missão.
- Conversar com a `esp_slave` por UART (enviar comandos, receber ACK/DONE/ABORT).
- Odometria/telemetria.

Ver [`../.ia/roadmap.md`](../.ia/roadmap.md) e [`../.ia/contexto-projeto.md`](../.ia/contexto-projeto.md).
