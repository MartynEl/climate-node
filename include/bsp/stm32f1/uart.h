#ifndef BSP_STM32F1_UART_H
#define BSP_STM32F1_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void uart1_init(void);

size_t uart1_tx_bytes(const uint8_t *data, size_t len);
void uart1_task(void);
uint32_t uart1_dropped_bytes(void);

bool uart1_rx_pop(void *ctx, uint8_t *out);
uint32_t uart1_rx_overflow_count(void);

#endif // BSP_STM32F1_UART_H
