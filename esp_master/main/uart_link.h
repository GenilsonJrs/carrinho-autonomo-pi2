#ifndef UART_LINK_H
#define UART_LINK_H

#include <stdint.h>
#include <stdbool.h>

#define SIG_ACK    0xFFFF
#define SIG_DONE   0xFFFE
#define SIG_ABORT  0xFFFD
#define CMD_STOP   0xFFFC
#define MAN_FRENTE 0xFFF0
#define MAN_RE     0xFFF1
#define MAN_ESQ    0xFFF2
#define MAN_DIR    0xFFF3

void uart_link_init(void);
void uart_link_flush(void);
void uart_link_send(uint16_t val);
bool uart_link_read(uint16_t *val, int timeout_ms);

#endif
