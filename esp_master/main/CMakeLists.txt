idf_component_register(SRCS "main.c"
                            "wifi_sta.c"
                            "web_server.c"
                            "fila.c"
                            "uart_link.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi
                                esp_event
                                esp_netif
                                nvs_flash
                                esp_http_server
                                json
                                esp_driver_uart
                       EMBED_FILES "interface/index.html"
                                   "interface/app.js"
                                   "interface/style.css")
