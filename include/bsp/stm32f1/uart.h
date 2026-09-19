#ifndef BSP_STM32F1_UART_H
#define BSP_STM32F1_UART_H

#include <stddef.h>
#include <stdint.h>

void uart1_init(void);

/*
 * Copies bytes into the TX ring buffer.
 * Returns number of bytes accepted.
 * Does not block.
 */
size_t uart1_tx_bytes(const uint8_t *data, size_t len);

/*
 * Drains the TX ring buffer into USART1 data register.
 * Call periodically from main loop / scheduler.
 */
void uart1_task(void);

uint32_t uart1_dropped_bytes(void);

#endif // BSP_STM32F1_UART_H
