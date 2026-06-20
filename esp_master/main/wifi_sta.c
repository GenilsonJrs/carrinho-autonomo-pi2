#include "wifi_sta.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

static const char *TAG = "WIFI";

bool wifi_ap_init(const char *ssid, const char *password) {
    if (ssid == NULL || password == NULL) return false;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wc = { 0 };
    strncpy((char *)wc.ap.ssid, ssid, sizeof(wc.ap.ssid) - 1);
    wc.ap.ssid_len = strlen(ssid);
    strncpy((char *)wc.ap.password, password, sizeof(wc.ap.password) - 1);
    wc.ap.channel = 1;
    wc.ap.max_connection = 4;
    wc.ap.authmode = (strlen(password) >= 8) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("\n===========================================================\n");
    printf("  REDE DO CARRINHO: \"%s\"\n", ssid);
    printf("  Conecte e abra:   http://192.168.4.1/\n");
    printf("===========================================================\n\n");
    ESP_LOGI(TAG, "SoftAP \"%s\" no ar (192.168.4.1)", ssid);
    return true;
}
