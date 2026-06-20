#include "wifi_sta.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     10

#define IP_FIXO   "192.168.1.200"
#define GW_FIXO   "192.168.1.1"
#define MASK_FIXO "255.255.255.0"

static EventGroupHandle_t s_group = NULL;
static int s_retry = 0;

static void handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        printf("\n===========================================================\n");
        printf("  INTERFACE WEB: http://" IP_FIXO "/\n");
        printf("===========================================================\n\n");
        s_retry = 0;
        xEventGroupSetBits(s_group, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry++;
            ESP_LOGW(TAG, "Reconectando (%d/%d)", s_retry, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_group, WIFI_FAIL_BIT);
        }
    }
}

bool wifi_sta_init(const char *ssid, const char *password) {
    if (ssid == NULL || password == NULL) return false;

    s_group = xEventGroupCreate();
    if (s_group == NULL) return false;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    esp_netif_dhcpc_stop(netif);
    esp_netif_ip_info_t ipinfo = { 0 };
    ipinfo.ip.addr      = esp_ip4addr_aton(IP_FIXO);
    ipinfo.gw.addr      = esp_ip4addr_aton(GW_FIXO);
    ipinfo.netmask.addr = esp_ip4addr_aton(MASK_FIXO);
    esp_netif_set_ip_info(netif, &ipinfo);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t a, b;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handler, NULL, &a));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler, NULL, &b));

    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, password, sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a \"%s\"...", ssid);
    EventBits_t bits = xEventGroupWaitBits(s_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
