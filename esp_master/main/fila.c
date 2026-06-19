#include "fila.h"

void fila_init(fila_t *f) {
    f->head = 0;
    f->tail = 0;
    f->mutex = xSemaphoreCreateMutex();
}

bool fila_push(fila_t *f, uint16_t cmd) {
    if (f->mutex == NULL) return false;
    xSemaphoreTake(f->mutex, portMAX_DELAY);
    bool ok = false;
    if ((uint16_t)((f->tail + 1) % MAX_FILA) != f->head) {
        f->cmd[f->tail] = cmd;
        f->tail = (f->tail + 1) % MAX_FILA;
        ok = true;
    }
    xSemaphoreGive(f->mutex);
    return ok;
}

bool fila_pop(fila_t *f, uint16_t *cmd) {
    if (f->mutex == NULL) return false;
    xSemaphoreTake(f->mutex, portMAX_DELAY);
    bool ok = false;
    if (f->head != f->tail) {
        *cmd = f->cmd[f->head];
        f->head = (f->head + 1) % MAX_FILA;
        ok = true;
    }
    xSemaphoreGive(f->mutex);
    return ok;
}

bool fila_vazia(fila_t *f) {
    return f->head == f->tail;
}

uint16_t fila_tam(fila_t *f) {
    if (f->tail >= f->head) return f->tail - f->head;
    return (MAX_FILA - f->head) + f->tail;
}

void fila_limpa(fila_t *f) {
    if (f->mutex == NULL) return;
    xSemaphoreTake(f->mutex, portMAX_DELAY);
    f->head = 0;
    f->tail = 0;
    xSemaphoreGive(f->mutex);
}
