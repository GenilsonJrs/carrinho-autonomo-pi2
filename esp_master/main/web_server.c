#include "web_server.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "uart_link.h"

static const char *TAG = "WEB";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t app_js_start[]      asm("_binary_app_js_start");
extern const uint8_t app_js_end[]        asm("_binary_app_js_end");
extern const uint8_t style_css_start[]   asm("_binary_style_css_start");
extern const uint8_t style_css_end[]     asm("_binary_style_css_end");

static web_ctx_t *g_ctx = NULL;

static void cors(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static esp_err_t h_options(httpd_req_t *req) {
    cors(req);
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t serve(httpd_req_t *req, const char *type, const uint8_t *a, const uint8_t *b) {
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, (const char *)a, b - a);
}

static esp_err_t h_index(httpd_req_t *req) { return serve(req, "text/html", index_html_start, index_html_end); }
static esp_err_t h_appjs(httpd_req_t *req) { return serve(req, "application/javascript", app_js_start, app_js_end); }
static esp_err_t h_css(httpd_req_t *req)   { return serve(req, "text/css", style_css_start, style_css_end); }

static esp_err_t h_status(httpd_req_t *req) {
    cors(req);
    char out[64];
    snprintf(out, sizeof(out), "{\"fila\":%u}", (unsigned)fila_tam(g_ctx->fila));
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, out);
}

static esp_err_t h_route(httpd_req_t *req) {
    cors(req);
    int total = req->content_len;
    if (total <= 0 || total > 8192) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "tamanho invalido");
        return ESP_FAIL;
    }
    char *buf = malloc(total + 1);
    if (!buf) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sem memoria"); return ESP_FAIL; }
    int got = 0;
    while (got < total) {
        int r = httpd_req_recv(req, buf + got, total - got);
        if (r <= 0) { free(buf); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv"); return ESP_FAIL; }
        got += r;
    }
    buf[total] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "json invalido"); return ESP_FAIL; }

    cJSON *op = cJSON_GetObjectItem(root, "operacao");
    if (op && cJSON_IsString(op) && strcmp(op->valuestring, "restart") == 0) {
        fila_limpa(g_ctx->fila);
        *g_ctx->emergencia = false;
    }

    int enf = 0;
    cJSON *rota = cJSON_GetObjectItem(root, "rota");
    if (cJSON_IsArray(rota)) {
        cJSON *a = NULL;
        cJSON_ArrayForEach(a, rota) {
            cJSON *jt = cJSON_GetObjectItem(a, "tipo");
            cJSON *jv = cJSON_GetObjectItem(a, "valor");
            cJSON *jd = cJSON_GetObjectItem(a, "direcao");
            if (!cJSON_IsString(jt) || !cJSON_IsNumber(jv)) continue;

            int valor = jv->valueint;
            if (valor < 0) valor = 0;
            if (valor > 0x3FFF) valor = 0x3FFF;

            int estado;
            if (strcmp(jt->valuestring, "girar") == 0) {
                const char *dir = (jd && cJSON_IsString(jd)) ? jd->valuestring : "clockwise";
                estado = (strcmp(dir, "anticlockwise") == 0 || strcmp(dir, "esquerda") == 0) ? 1 : 0;
            } else {
                const char *dir = (jd && cJSON_IsString(jd)) ? jd->valuestring : "forward";
                estado = (strcmp(dir, "backward") == 0 || strcmp(dir, "re") == 0) ? 2 : 3;
            }
            uint16_t cmd = ((uint16_t)(estado & 0x03) << 14) | (uint16_t)(valor & 0x3FFF);
            if (fila_push(g_ctx->fila, cmd)) enf++;
        }
    }
    cJSON_Delete(root);

    char out[80];
    snprintf(out, sizeof(out), "{\"ok\":true,\"enfileirados\":%d,\"fila\":%u}", enf, (unsigned)fila_tam(g_ctx->fila));
    httpd_resp_set_type(req, "application/json");
    ESP_LOGI(TAG, "Rota: +%d (fila=%u)", enf, (unsigned)fila_tam(g_ctx->fila));
    return httpd_resp_sendstr(req, out);
}

static esp_err_t h_emergency(httpd_req_t *req) {
    cors(req);
    uart_link_send(CMD_STOP);
    fila_limpa(g_ctx->fila);
    *g_ctx->emergencia = true;
    ESP_LOGW(TAG, "EMERGENCIA: STOP enviado + fila limpa");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t h_manual(httpd_req_t *req) {
    cors(req);
    char buf[8] = {0};
    int n = httpd_req_recv(req, buf, sizeof(buf) - 1);
    char c = (n > 0) ? buf[0] : 'S';
    uint16_t frame;
    switch (c) {
        case 'F': frame = MAN_FRENTE; break;
        case 'B': frame = MAN_RE;     break;
        case 'L': frame = MAN_ESQ;    break;
        case 'R': frame = MAN_DIR;    break;
        default:  frame = CMD_STOP;   break;
    }
    uart_link_send(frame);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

bool web_server_start(web_ctx_t *ctx) {
    if (!ctx || !ctx->fila) return false;
    g_ctx = ctx;

    httpd_handle_t server = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 14;
    cfg.lru_purge_enable = true;
    if (httpd_start(&server, &cfg) != ESP_OK) return false;

    const httpd_uri_t routes[] = {
        { .uri = "/",              .method = HTTP_GET,  .handler = h_index },
        { .uri = "/index.html",    .method = HTTP_GET,  .handler = h_index },
        { .uri = "/app.js",        .method = HTTP_GET,  .handler = h_appjs },
        { .uri = "/style.css",     .method = HTTP_GET,  .handler = h_css },
        { .uri = "/api/status",    .method = HTTP_GET,  .handler = h_status },
        { .uri = "/api/route",     .method = HTTP_POST, .handler = h_route },
        { .uri = "/api/emergency", .method = HTTP_POST, .handler = h_emergency },
        { .uri = "/api/route",     .method = HTTP_OPTIONS, .handler = h_options },
        { .uri = "/api/emergency", .method = HTTP_OPTIONS, .handler = h_options },
        { .uri = "/api/manual",    .method = HTTP_POST, .handler = h_manual },
        { .uri = "/api/manual",    .method = HTTP_OPTIONS, .handler = h_options },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
    ESP_LOGI(TAG, "HTTP server iniciado");
    return true;
}
