#ifndef FILA_H
#define FILA_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_FILA 256

typedef struct {
    uint16_t cmd[MAX_FILA];
    uint16_t head;
    uint16_t tail;
    SemaphoreHandle_t mutex;
} fila_t;

void     fila_init(fila_t *f);
bool     fila_push(fila_t *f, uint16_t cmd);
bool     fila_pop(fila_t *f, uint16_t *cmd);
bool     fila_vazia(fila_t *f);
uint16_t fila_tam(fila_t *f);
void     fila_limpa(fila_t *f);

#endif
