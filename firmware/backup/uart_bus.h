#ifndef _UART_BUS_H
#define _UART_BUS_H

#include "config.h"

void setup_uart_bus();
void uart_bus_poll();
void process_packet(uint8_t *buf, size_t len, USART_TypeDef *src_uart);

#endif
