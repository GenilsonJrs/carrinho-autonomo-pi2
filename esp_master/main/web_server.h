#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include "fila.h"

typedef struct {
    fila_t        *fila;
    volatile bool *emergencia;
} web_ctx_t;

bool web_server_start(web_ctx_t *ctx);

#endif
