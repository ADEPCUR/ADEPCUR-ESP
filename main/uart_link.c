#include "uart_link.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// Reusando los pines por defecto (TX=1, RX=3)
#define UART_NUM UART_NUM_0

void uart_link_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // Inicializar UART0 forzando los pines 1 (TX) y 3 (RX)
    uart_driver_install(UART_NUM, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void uart_link_send(const uint8_t *data, int len) {
    uart_write_bytes(UART_NUM, (const char *)data, len);
}
