#ifndef UART_LINK_H
#define UART_LINK_H

#include <stdint.h>

void uart_link_init(void);
void uart_link_send(const uint8_t *data, int len);

#endif
