#include "bsp/stm32f1/uart.h"

#include <stddef.h>
#include <stdint.h>

/*
 * STM32F103 USART1 registers.
 *
 * This is intentionally minimal and register-level.
 * Later this can be replaced by HAL/LL or DMA implementation.
 */
#define RCC_BASE            0x40021000UL
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18UL))

#define RCC_APB2ENR_IOPAEN  (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 14)

#define GPIOA_BASE          0x40010800UL
#define GPIOA_CRH           (*(volatile uint32_t *)(GPIOA_BASE + 0x04UL))

#define USART1_BASE         0x40013800UL
#define USART1_SR           (*(volatile uint32_t *)(USART1_BASE + 0x00UL))
#define USART1_DR           (*(volatile uint32_t *)(USART1_BASE + 0x04UL))
#define USART1_BRR          (*(volatile uint32_t *)(USART1_BASE + 0x08UL))
#define USART1_CR1          (*(volatile uint32_t *)(USART1_BASE + 0x0CUL))
#define USART1_CR2          (*(volatile uint32_t *)(USART1_BASE + 0x10UL))
#define USART1_CR3          (*(volatile uint32_t *)(USART1_BASE + 0x14UL))

#define USART1_SR_TXE       (1UL << 7)

#define USART1_CR1_UE       (1UL << 13)
#define USART1_CR1_TE       (1UL << 3)

/*
 * Default clock in this skeleton is HSI 8 MHz.
 * 115200 baud, oversampling by 16:
 *
 *   8000000 / 115200 = 69.444...
 *   mantissa = 69
 *   fraction = round(0.444 * 16) = 7
 *   BRR = (69 << 4) | 7 = 0x457
 */
#define USART1_BRR_115200_8MHZ 0x457UL

#define UART1_TX_RING_SIZE 128u
#define UART1_TX_RING_MASK (UART1_TX_RING_SIZE - 1u)
#define UART1_TX_MAX_PER_TASK 16u

static uint8_t g_tx_ring[UART1_TX_RING_SIZE];
static size_t g_tx_head = 0u;
static size_t g_tx_tail = 0u;
static size_t g_tx_count = 0u;
static uint32_t g_tx_dropped = 0u;

void uart1_init(void)
{
    /*
     * Enable GPIOA and USART1 clocks.
     */
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /*
     * Configure PA9 as USART1_TX.
     *
     * PA9 is controlled by CRH bits [7:4].
     * Mode: 50 MHz output.
     * Configuration: alternate function push-pull.
     * Value: 0xB.
     */
    uint32_t crh = GPIOA_CRH;
    crh &= ~(0xFUL << 4);
    crh |= (0xBUL << 4);
    GPIOA_CRH = crh;

    /*
     * USART1 configuration:
     *   115200 8N1
     *   transmitter enabled
     */
    USART1_BRR = USART1_BRR_115200_8MHZ;
    USART1_CR2 = 0u;
    USART1_CR3 = 0u;
    USART1_CR1 = USART1_CR1_UE | USART1_CR1_TE;
}

size_t uart1_tx_bytes(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0u) {
        return 0u;
    }

    size_t accepted = 0u;

    for (size_t i = 0u; i < len; ++i) {
        if (g_tx_count >= UART1_TX_RING_SIZE) {
            g_tx_dropped++;
            continue;
        }

        g_tx_ring[g_tx_head] = data[i];
        g_tx_head = (g_tx_head + 1u) & UART1_TX_RING_MASK;
        g_tx_count++;
        accepted++;
    }

    return accepted;
}

void uart1_task(void)
{
    uint32_t sent = 0u;

    while (g_tx_count > 0u && sent < UART1_TX_MAX_PER_TASK) {
        if ((USART1_SR & USART1_SR_TXE) == 0u) {
            break;
        }

        USART1_DR = g_tx_ring[g_tx_tail];
        g_tx_tail = (g_tx_tail + 1u) & UART1_TX_RING_MASK;
        g_tx_count--;
        sent++;
    }
}

uint32_t uart1_dropped_bytes(void)
{
    return g_tx_dropped;
}
