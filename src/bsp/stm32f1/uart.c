#include "bsp/stm32f1/uart.h"
#include <stddef.h>
#include <stdint.h>

/* 
 * STM32F103 USART1 registers.
 */
#define RCC_BASE             0x40021000UL
#define RCC_APB2ENR          (*(volatile uint32_t *)(RCC_BASE + 0x18UL))
#define RCC_APB2ENR_IOPAEN   (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 14)

#define GPIOA_BASE           0x40010800UL
#define GPIOA_CRH            (*(volatile uint32_t *)(GPIOA_BASE + 0x04UL))

#define USART1_BASE          0x40013800UL
#define USART1_SR            (*(volatile uint32_t *)(USART1_BASE + 0x00UL))
#define USART1_DR            (*(volatile uint32_t *)(USART1_BASE + 0x04UL))
#define USART1_BRR           (*(volatile uint32_t *)(USART1_BASE + 0x08UL))
#define USART1_CR1           (*(volatile uint32_t *)(USART1_BASE + 0x0CUL))
#define USART1_CR2           (*(volatile uint32_t *)(USART1_BASE + 0x10UL))
#define USART1_CR3           (*(volatile uint32_t *)(USART1_BASE + 0x14UL))

#define USART1_SR_RXNE       (1UL << 5)
#define USART1_SR_ORE        (1UL << 3)
#define USART1_SR_TXE        (1UL << 7)
#define USART1_SR_TC         (1UL << 6) /* Transmission Complete */

#define USART1_CR1_UE        (1UL << 13)
#define USART1_CR1_TE        (1UL << 3)
#define USART1_CR1_RE        (1UL << 2)
#define USART1_CR1_RXNEIE    (1UL << 5)

/*
 * NVIC interrupt set enable register 1: IRQ 32..63.
 * USART1_IRQn for STM32F103 is 37.
 */
#define NVIC_ISER1           (*(volatile uint32_t *)0xE000E104UL)
#define USART1_IRQ_BIT       (1UL << (37UL - 32UL))

/*
 * Default clock in this skeleton is HSI 8 MHz.
 * 115200 baud, oversampling by 16:
 *   BRR = 0x457
 */
#define USART1_BRR_115200_8MHZ 0x457UL

#define UART1_TX_RING_SIZE     128u
#define UART1_TX_RING_MASK     (UART1_TX_RING_SIZE - 1u)
#define UART1_TX_MAX_PER_TASK  16u
#define UART1_RX_RING_SIZE     256u
#define UART1_RX_RING_MASK     (UART1_RX_RING_SIZE - 1u)

static uint8_t g_tx_ring[UART1_TX_RING_SIZE];
static size_t g_tx_head = 0u;
static size_t g_tx_tail = 0u;
static size_t g_tx_count = 0u;
static uint32_t g_tx_dropped = 0u;

static uint8_t g_rx_ring[UART1_RX_RING_SIZE];
static volatile uint16_t g_rx_head = 0u;
static volatile uint16_t g_rx_tail = 0u;
static volatile uint32_t g_rx_overflow = 0u;

/* --- RS-485 Half-Duplex Control --- */
static bool g_rs485_mode = false;
static volatile uint32_t *g_gpio_bsrr_ptr = NULL; // Points to GPIOx_BSRR
static volatile uint32_t *g_gpio_brr_ptr = NULL;  // Points to GPIOx_BRR
static uint32_t g_de_pin_mask = 0u;               // Bit mask for DE pin (e.g., 1<<0 for PA0/PB0/etc)
static bool g_transmission_in_progress = false;

/**
 * Configure UART1 for RS-485 operation.
 * @param gpio_base_address Base address of the GPIO port controlling DE/RE (e.g., GPIOB_BASE)
 * @param de_pin_number Pin number on that port (0-15)
 */
void uart1_set_rs485_pin(uint32_t gpio_base_address, uint32_t de_pin_number) {
    if (de_pin_number > 15u) return;
    
    g_de_pin_mask = (1UL << de_pin_number);
    
    // Calculate addresses based on standard STM32F1 GPIO layout
    // BSRR is at offset 0x0C, BRR is at offset 0x10 from GPIO Base
    g_gpio_bsrr_ptr = (volatile uint32_t *)(gpio_base_address + 0x0CU);
    g_gpio_brr_ptr  = (volatile uint32_t *)(gpio_base_address + 0x10U);
    
    g_rs485_mode = true;
    
    // Ensure receiver is enabled initially (DE low)
    if (g_gpio_brr_ptr != NULL) {
        *g_gpio_brr_ptr = g_de_pin_mask;
    }
}

void uart1_init(void)
{
    /*
     * Enable GPIOA and USART1 clocks.
     */
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /*
     * PA9 = USART1_TX:
     *   50 MHz alternate function push-pull => 0xB in CRH nibble for pin 9.
     *
     * PA10 = USART1_RX:
     *   floating input => 0x4 in CRH nibble for pin 10.
     */
    uint32_t crh = GPIOA_CRH;
    crh &= ~(0xFUL << 4);
    crh |= (0xBUL << 4);
    crh &= ~(0xFUL << 8);
    crh |= (0x4UL << 8);
    GPIOA_CRH = crh;

    g_tx_head = 0u;
    g_tx_tail = 0u;
    g_tx_count = 0u;
    g_tx_dropped = 0u;
    g_rx_head = 0u;
    g_rx_tail = 0u;
    g_rx_overflow = 0u;
    g_transmission_in_progress = false;

    USART1_BRR = USART1_BRR_115200_8MHZ;
    USART1_CR2 = 0u;
    USART1_CR3 = 0u;
    USART1_CR1 = USART1_CR1_UE | USART1_CR1_TE | USART1_CR1_RE | USART1_CR1_RXNEIE;
    NVIC_ISER1 |= USART1_IRQ_BIT;
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
    
    // In RS-485 mode, switching happens inside uart1_task when actually sending bytes.
    // This prevents race conditions where multiple calls toggle pins rapidly.
    
    return accepted;
}

void uart1_task(void)
{
    uint32_t sent = 0u;
    
    // Handle RS-485 Line State Management
    
    if (g_rs485_mode && g_gpio_bsrr_ptr != NULL) {
        
        // Scenario 1: We have data to send but haven't switched to TX yet
        if (!g_transmission_in_progress && g_tx_count > 0u) {
            // Switch to Transmit Mode (Set DE High)
            *g_gpio_bsrr_ptr = g_de_pin_mask; 
            
            // Small delay might be needed depending on hardware capacitance, 
            // usually negligible for MAX3485 at these speeds, but good practice to note.
            
            g_transmission_in_progress = true;
        }
        
        // Scenario 2: We are transmitting
        if (g_transmission_in_progress) {
            while (g_tx_count > 0u && sent < UART1_TX_MAX_PER_TASK) {
                if ((USART1_SR & USART1_SR_TXE) == 0u) {
                    break; // Wait for Data Register Empty
                }
                USART1_DR = g_tx_ring[g_tx_tail];
                g_tx_tail = (g_tx_tail + 1u) & UART1_TX_RING_MASK;
                g_tx_count--;
                sent++;
            }
            
            // Check if buffer is empty AND transmission is physically complete
            if (g_tx_count == 0u) {
                // Must wait for TC (Transmission Complete) flag before disabling DE
                // Otherwise last byte gets truncated
                if ((USART1_SR & USART1_SR_TC) != 0u) {
                    // Switch back to Receive Mode (Clear DE / Set RE)
                    *g_gpio_brr_ptr = g_de_pin_mask;
                    g_transmission_in_progress = false;
                    
                    // Clear TC flag manually if necessary? Usually auto-cleared on next write, 
                    // but reading SR clears ORE/RXNE. TC needs careful handling.
                    // Writing to DR resets TC. Since we stopped writing, we rely on it staying set until next start.
                    // Some implementations clear it explicitly:
                    // USART1_SR &= ~USART1_SR_TC; // Not recommended as it's read-only in some docs, better to ignore or handle via flow control.
                }
            }
        }
    } else {
        // Standard Full-Duplex Operation (Original Code)
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
}

uint32_t uart1_dropped_bytes(void)
{
    return g_tx_dropped;
}

bool uart1_rx_pop(void *ctx, uint8_t *out)
{
    (void)ctx;
    if (out == NULL) {
        return false;
    }
    uint16_t tail = g_rx_tail;
    if (tail == g_rx_head) {
        return false;
    }
    *out = g_rx_ring[tail];
    g_rx_tail = (uint16_t)((tail + 1u) & UART1_RX_RING_MASK);
    return true;
}

uint32_t uart1_rx_overflow_count(void)
{
    return g_rx_overflow;
}

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1_SR;
    if ((sr & (USART1_SR_RXNE | USART1_SR_ORE)) != 0u) {
        uint8_t byte = (uint8_t)(USART1_DR & 0xFFu);
        if ((sr & USART1_SR_ORE) != 0u) {
            g_rx_overflow++;
        }
        uint16_t head = g_rx_head;
        uint16_t next = (uint16_t)((head + 1u) & UART1_RX_RING_MASK);
        if (next != g_rx_tail) {
            g_rx_ring[head] = byte;
            g_rx_head = next;
        } else {
            g_rx_overflow++;
        }
    }
}