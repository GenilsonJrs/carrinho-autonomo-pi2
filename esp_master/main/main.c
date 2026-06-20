#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "wifi_sta.h"
#include "web_server.h"
#include "fila.h"
#include "uart_link.h"

static const char *TAG = "MASTER";

#define ACK_TIMEOUT_MS   300
#define ACK_RETRIES      5
#define DONE_TIMEOUT_MS  180000

#define AP_SSID "Carrinho_BB8"
#define AP_PASS "bb812345"

static fila_t        g_fila;
static volatile bool g_emergencia = false;

static bool esperar_done(void) {
    int passado = 0;
    while (passado < DONE_TIMEOUT_MS) {
        if (g_emergencia) return false;
        uint16_t sig;
        if (uart_link_read(&sig, 200)) {
            if (sig == SIG_DONE)  return true;
            if (sig == SIG_ABORT) { ESP_LOGW(TAG, "Escravo: ABORT"); fila_limpa(&g_fila); return false; }
        }
        passado += 200;
    }
    ESP_LOGW(TAG, "Timeout esperando DONE");
    return false;
}

static void task_despacho(void *arg) {
    for (;;) {
        if (g_emergencia) {
            fila_limpa(&g_fila);
            g_emergencia = false;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        uint16_t cmd;
        if (!fila_pop(&g_fila, &cmd)) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        bool ack = false;
        for (int i = 0; i < ACK_RETRIES && !ack && !g_emergencia; i++) {
            uart_link_flush();
            uart_link_send(cmd);
            uint16_t sig;
            if (uart_link_read(&sig, ACK_TIMEOUT_MS) && sig == SIG_ACK) ack = true;
            else ESP_LOGW(TAG, "Sem ACK (%d/%d), reenviando", i + 1, ACK_RETRIES);
        }

        if (!ack) {
            ESP_LOGE(TAG, "Escravo nao respondeu. Limpando fila.");
            fila_limpa(&g_fila);
            continue;
        }

        ESP_LOGI(TAG, "Comando 0x%04X aceito; aguardando conclusao", cmd);
        if (esperar_done()) {
            ESP_LOGI(TAG, "Comando 0x%04X concluido", cmd);
        }
    }
}

static void task_heartbeat(void *arg) {
    for (;;) {
        wifi_sta_list_t sl;
        int n = (esp_wifi_ap_get_sta_list(&sl) == ESP_OK) ? sl.num : -1;
        ESP_LOGI(TAG, "AP \"%s\" no ar | http://192.168.4.1/ | clientes=%d", AP_SSID, n);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    fila_init(&g_fila);
    uart_link_init();

    if (!wifi_ap_init(AP_SSID, AP_PASS)) {
        ESP_LOGE(TAG, "Falha ao subir o ponto de acesso");
        return;
    }

    static web_ctx_t ctx;
    ctx.fila = &g_fila;
    ctx.emergencia = &g_emergencia;
    if (!web_server_start(&ctx)) {
        ESP_LOGE(TAG, "Falha ao iniciar servidor web");
        return;
    }

    xTaskCreate(task_despacho, "despacho", 4096, NULL, 6, NULL);
    xTaskCreate(task_heartbeat, "heartbeat", 3072, NULL, 3, NULL);
    ESP_LOGI(TAG, "Mestre pronto: web + UART para o escravo.");
}
