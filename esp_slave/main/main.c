#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BB8";

#define M1_R_PWM 4
#define M1_L_PWM 5
#define M1_REN   7
#define M1_LEN   15
#define M2_R_PWM 1
#define M2_L_PWM 2
#define M2_REN   41
#define M2_LEN   40

#define VEL_RETA  200
#define VEL_GIRO  170

#define CTRL_MS    50
#define VEL_MIN    90
#define KP_RUMO    0.30f
#define KI_RUMO    0.004f
#define INTEG_MAX  8000.0f
#define CORR_MAX   70.0f

#define ROTA_PULSOS_RETA  7400
#define ROTA_PULSOS_GIRO  435

#define LEDC_FREQ_HZ 20000
#define LEDC_RES     LEDC_TIMER_8_BIT
#define CH_M1_R LEDC_CHANNEL_0
#define CH_M1_L LEDC_CHANNEL_1
#define CH_M2_R LEDC_CHANNEL_2
#define CH_M2_L LEDC_CHANNEL_3

#define PINO_QUEDA   GPIO_NUM_6
#define PINO_QUEDA2  GPIO_NUM_8
#define DEBOUNCE_N   3

#define ENC_ESQ_PIN  GPIO_NUM_10
#define ENC_DIR_PIN  GPIO_NUM_9

static pcnt_unit_handle_t pcnt_esq = NULL;
static pcnt_unit_handle_t pcnt_dir = NULL;
static volatile bool g_queda = false;
static volatile char g_cmd   = 'S';
static uint8_t own_addr_type;
static void start_advertising(void);

static void set_duty(ledc_channel_t ch, uint32_t duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static void aplicarPWM(int pwmEsq, int pwmDir) {
    if (pwmEsq >= 0) { set_duty(CH_M1_R, 0);       set_duty(CH_M1_L, pwmEsq); }
    else             { set_duty(CH_M1_R, -pwmEsq); set_duty(CH_M1_L, 0); }
    if (pwmDir >= 0) { set_duty(CH_M2_R, pwmDir);  set_duty(CH_M2_L, 0); }
    else             { set_duty(CH_M2_R, 0);       set_duty(CH_M2_L, -pwmDir); }
}

static void motores_stop(void) { aplicarPWM(0, 0); }

static void init_motores(void) {
    gpio_config_t io = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << M1_REN) | (1ULL << M1_LEN) | (1ULL << M2_REN) | (1ULL << M2_LEN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(M1_REN, 1); gpio_set_level(M1_LEN, 1);
    gpio_set_level(M2_REN, 1); gpio_set_level(M2_LEN, 1);

    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = LEDC_RES,
        .freq_hz         = LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    const int            pins[4] = { M1_R_PWM, M1_L_PWM, M2_R_PWM, M2_L_PWM };
    const ledc_channel_t chs[4]  = { CH_M1_R, CH_M1_L, CH_M2_R, CH_M2_L };
    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t c = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = chs[i],
            .timer_sel  = LEDC_TIMER_0,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = pins[i],
            .duty       = 0,
            .hpoint     = 0,
        };
        ledc_channel_config(&c);
    }
    motores_stop();
    ESP_LOGI(TAG, "Motores OK");
}

static void handle_command(char cmd) {
    g_cmd = cmd;
    ESP_LOGI(TAG, "CMD %c", cmd);
}

static void init_sensores(void) {
    gpio_config_t io = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PINO_QUEDA) | (1ULL << PINO_QUEDA2),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    ESP_LOGI(TAG, "Sensor queda GPIO%d", PINO_QUEDA);
}

static void task_sensor_queda(void *arg) {
    int estavel = gpio_get_level(PINO_QUEDA);
    int cont    = 0;
    g_queda = (estavel == 1);
    for (;;) {
        int raw = gpio_get_level(PINO_QUEDA);
        if (raw != estavel) {
            if (++cont >= DEBOUNCE_N) {
                estavel = raw;
                cont    = 0;
                g_queda = (estavel == 1);
                ESP_LOGW(TAG, "%s", g_queda ? "QUEDA" : "CHAO OK");
            }
        } else {
            cont = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static pcnt_unit_handle_t criar_pcnt(int gpio) {
    pcnt_unit_handle_t unit = NULL;
    pcnt_unit_config_t ucfg = { .high_limit = 30000, .low_limit = -1 };
    if (pcnt_new_unit(&ucfg, &unit) != ESP_OK) return NULL;
    pcnt_glitch_filter_config_t fcfg = { .max_glitch_ns = 1000 };
    pcnt_unit_set_glitch_filter(unit, &fcfg);
    pcnt_chan_config_t ccfg = { .edge_gpio_num = gpio, .level_gpio_num = -1 };
    pcnt_channel_handle_t chan = NULL;
    if (pcnt_new_channel(unit, &ccfg, &chan) != ESP_OK) return NULL;
    gpio_set_pull_mode(gpio, GPIO_PULLUP_ONLY);
    pcnt_channel_set_edge_action(chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
    pcnt_unit_enable(unit);
    pcnt_unit_clear_count(unit);
    pcnt_unit_start(unit);
    return unit;
}

static void init_encoders(void) {
    pcnt_esq = criar_pcnt(ENC_ESQ_PIN);
    pcnt_dir = criar_pcnt(ENC_DIR_PIN);
    ESP_LOGI(TAG, "Encoders %s", (pcnt_esq && pcnt_dir) ? "OK" : "FALHA");
}

static void controle_reta(int dL, int dR, float *rumo, float *integ, int *magL, int *magR) {
    *rumo  += (float)(dL - dR);
    *integ += *rumo;
    if (*integ >  INTEG_MAX) *integ =  INTEG_MAX;
    if (*integ < -INTEG_MAX) *integ = -INTEG_MAX;
    float corr = KP_RUMO * (*rumo) + KI_RUMO * (*integ);
    if (corr >  CORR_MAX) corr =  CORR_MAX;
    if (corr < -CORR_MAX) corr = -CORR_MAX;
    int L = (int)(VEL_RETA - corr);
    int R = (int)(VEL_RETA + corr);
    if (L < VEL_MIN) L = VEL_MIN;
    if (L > 255)     L = 255;
    if (R < VEL_MIN) R = VEL_MIN;
    if (R > 255)     R = 255;
    *magL = L;
    *magR = R;
}

static void task_controle(void *arg) {
    long  total_esq = 0, total_dir = 0;
    float rumo = 0.0f, integ = 0.0f;
    char  modo_ant = 'S';
    int   rota_fase = 0;
    float rota_pulsos = 0.0f;
    int   tick = 0;

    for (;;) {
        int dL = 0, dR = 0;
        if (pcnt_esq) { pcnt_unit_get_count(pcnt_esq, &dL); pcnt_unit_clear_count(pcnt_esq); }
        if (pcnt_dir) { pcnt_unit_get_count(pcnt_dir, &dR); pcnt_unit_clear_count(pcnt_dir); }
        total_esq += dL;
        total_dir += dR;

        char c = g_cmd;
        if (g_queda && (c == 'F' || c == 'L' || c == 'R' || c == 'A')) c = 'S';

        char modo = (c == 'F') ? 'F'
                  : (c == 'B') ? 'B'
                  : (c == 'A') ? 'A'
                  : (c == 'L' || c == 'R') ? 'T'
                  : 'S';

        if (modo != modo_ant) {
            rumo = 0.0f; integ = 0.0f; rota_fase = 0; rota_pulsos = 0.0f;
            modo_ant = modo;
        }

        if (modo == 'F' || modo == 'B') {
            int magL, magR;
            controle_reta(dL, dR, &rumo, &integ, &magL, &magR);
            if (modo == 'F') aplicarPWM(magL, magR);
            else             aplicarPWM(-magL, -magR);
        } else if (modo == 'T') {
            if (c == 'L') aplicarPWM(VEL_GIRO, -VEL_GIRO);
            else          aplicarPWM(-VEL_GIRO, VEL_GIRO);
        } else if (modo == 'A') {
            rota_pulsos += (dL + dR) / 2.0f;
            if (rota_fase == 0) {
                int magL, magR;
                controle_reta(dL, dR, &rumo, &integ, &magL, &magR);
                aplicarPWM(magL, magR);
                if (rota_pulsos >= ROTA_PULSOS_RETA) {
                    rota_fase = 1;
                    rota_pulsos = 0.0f;
                    rumo = 0.0f; integ = 0.0f;
                }
            } else {
                aplicarPWM(-VEL_GIRO, VEL_GIRO);
                if (rota_pulsos >= ROTA_PULSOS_GIRO) {
                    rota_fase = 0;
                    rota_pulsos = 0.0f;
                    rumo = 0.0f; integ = 0.0f;
                }
            }
        } else {
            aplicarPWM(0, 0);
        }

        if (++tick >= (500 / CTRL_MS)) {
            tick = 0;
            ESP_LOGI(TAG, "dL=%d dR=%d totE=%ld totD=%ld modo=%c fase=%d",
                     dL, dR, total_esq, total_dir, modo, rota_fase);
        }
        vTaskDelay(pdMS_TO_TICKS(CTRL_MS));
    }
}

static int gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t  buf[8] = {0};
        uint16_t len    = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
        if (rc == 0 && len > 0) {
            handle_command((char)buf[0]);
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x00FF),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid      = BLE_UUID16_DECLARE(0xFF01),
                .access_cb = gatt_chr_access,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    { 0 }
};

static int gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Conexao %s", event->connect.status == 0 ? "OK" : "FALHOU");
            if (event->connect.status != 0) start_advertising();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGW(TAG, "Desconectado");
            g_cmd = 'S';
            motores_stop();
            start_advertising();
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            start_advertising();
            break;
        default:
            break;
    }
    return 0;
}

static void start_advertising(void) {
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    const char *name = ble_svc_gap_device_name();
    fields.name             = (uint8_t *)name;
    fields.name_len         = strlen(name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    ESP_LOGI(TAG, "Advertising '%s'", name);
}

static void on_sync(void) {
    ble_hs_id_infer_auto(0, &own_addr_type);
    start_advertising();
}

static void on_reset(int reason) {
    ESP_LOGW(TAG, "BLE reset %d", reason);
}

static void host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    init_motores();
    init_sensores();
    init_encoders();
    xTaskCreate(task_sensor_queda, "sensor_queda", 3072, NULL, 6, NULL);
    xTaskCreate(task_controle,     "controle",     4096, NULL, 5, NULL);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init falhou: %d", ret);
        return;
    }

    ble_svc_gap_device_name_set("ROBO_BB8");
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "BB8 slave iniciado. BLE 'ROBO_BB8'.");
}
