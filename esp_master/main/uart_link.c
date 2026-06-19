#include "uart_link.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_LINK_PORT  UART_NUM_1
#define UART_LINK_TX    17
#define UART_LINK_RX    16
#define UART_LINK_BAUD  115200

void uart_link_init(void) {
    uart_config_t cfg = {
        .baud_rate  = UART_LINK_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_LINK_PORT, 1024, 1024, 0, NULL, 0);
    uart_param_config(UART_LINK_PORT, &cfg);
    uart_set_pin(UART_LINK_PORT, UART_LINK_TX, UART_LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void uart_link_flush(void) {
    uart_flush_input(UART_LINK_PORT);
}

void uart_link_send(uint16_t val) {
    uint8_t d[3];
    d[0] = (uint8_t)(val >> 8);
    d[1] = (uint8_t)(val & 0xFF);
    d[2] = d[0] ^ d[1];
    uart_write_bytes(UART_LINK_PORT, (const char *)d, 3);
}

bool uart_link_read(uint16_t *val, int timeout_ms) {
    uint8_t rx[3];
    uint8_t idx = 0;
    TickType_t fim = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (xTaskGetTickCount() < fim) {
        uint8_t b;
        int n = uart_read_bytes(UART_LINK_PORT, &b, 1, pdMS_TO_TICKS(5));
        if (n <= 0) continue;
        rx[idx++] = b;
        if (idx == 3) {
            if ((rx[0] ^ rx[1]) == rx[2]) {
                *val = ((uint16_t)rx[0] << 8) | rx[1];
                return true;
            }
            rx[0] = rx[1];
            rx[1] = rx[2];
            idx = 2;
        }
    }
    return false;
}
