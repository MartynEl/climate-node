#include "bsp/stm32f1/uart.h"
#include <stddef.h>
#include <stdint.h>

/* --- Регистры DMA1 (STM32F103) --- */
#define DMA1_BASE             0x40020000UL
#define DMA1_ISR              (*(volatile uint32_t *)(DMA1_BASE + 0x00UL))
#define DMA1_IFCR             (*(volatile uint32_t *)(DMA1_BASE + 0x04UL))

// Канал 4 (USART1_TX)
#define DMA1_CCR4             (*(volatile uint32_t *)(DMA1_BASE + 0x44UL))
#define DMA1_CNDTR4           (*(volatile uint32_t *)(DMA1_BASE + 0x48UL))
#define DMA1_CPAR4            (*(volatile uint32_t *)(DMA1_BASE + 0x4CUL))
#define DMA1_CMAR4            (*(volatile uint32_t *)(DMA1_BASE + 0x50UL))

// Канал 5 (USART1_RX)
#define DMA1_CCR5             (*(volatile uint32_t *)(DMA1_BASE + 0x58UL))
#define DMA1_CNDTR5           (*(volatile uint32_t *)(DMA1_BASE + 0x5CUL))
#define DMA1_CPAR5            (*(volatile uint32_t *)(DMA1_BASE + 0x60UL))
#define DMA1_CMAR5            (*(volatile uint32_t *)(DMA1_BASE + 0x64UL))

/* --- Дополнения к регистрам USART1 --- */
#define USART1_BASE          0x40013800UL
#define USART1_SR            (*(volatile uint32_t *)(USART1_BASE + 0x00UL))
#define USART1_DR            (*(volatile uint32_t *)(USART1_BASE + 0x04UL))
#define USART1_CR1           (*(volatile uint32_t *)(USART1_BASE + 0x0CUL))
#define USART1_CR3           (*(volatile uint32_t *)(USART1_BASE + 0x14UL))

#define USART1_SR_IDLE       (1UL << 4)
#define USART1_CR1_IDLEIE    (1UL << 4)
#define USART1_CR3_DMAT      (1UL << 7) // Enable DMA for Transmitter
#define USART1_CR3_DMAR      (1UL << 6) // Enable DMA for Receiver

/* --- Регистры RCC для DMA --- */
#define RCC_BASE             0x40021000UL
#define RCC_AHBENR           (*(volatile uint32_t *)(RCC_BASE + 0x14UL))
#define RCC_AHBENR_DMA1EN    (1UL << 0)

/* --- Регистры NVIC (Добавилось прерывание DMA1) --- */
#define NVIC_ISER0           (*(volatile uint32_t *)0xE000E100UL)
#define NVIC_ISER1           (*(volatile uint32_t *)0xE000E104UL)
#define DMA1_CH4_IRQ_BIT     (1UL << 14) // IRQ 14 (Channel 4)
#define USART1_IRQ_BIT       (1UL << (37UL - 32UL)) // IRQ 37 (1 << 5 в ISER1)

/* --- Конфигурация буферов --- */
#define UART_RX_DMA_BUF_SIZE 256u
static uint8_t g_rx_dma_buffer[UART_RX_DMA_BUF_SIZE];
static size_t g_last_rx_pos = 0u;

static uart_callback_fn g_uart_callback = NULL;

/* --- Настройки RS-485 --- */
static bool g_rs485_mode = false;
static volatile uint32_t *g_gpio_bsrr_ptr = NULL;
static volatile uint32_t *g_gpio_brr_ptr = NULL;
static uint32_t g_de_pin_mask = 0u;

void uart1_set_rs485_pin(uint32_t gpio_base_address, uint32_t de_pin_number) {
    if (de_pin_number > 15u) return;
    g_de_pin_mask = (1UL << de_pin_number);
    g_gpio_bsrr_ptr = (volatile uint32_t *)(gpio_base_address + 0x0CU);
    g_gpio_brr_ptr  = (volatile uint32_t *)(gpio_base_address + 0x10U);
    g_rs485_mode = true;
    if (g_gpio_brr_ptr != NULL) *g_gpio_brr_ptr = g_de_pin_mask; // RE Mode active
}

/**
 * Асинхронная инициализация UART1 + DMA1
 */
void uart1_async_init(uart_callback_fn callback)
{
    g_uart_callback = callback;
    g_last_rx_pos = 0u;

    // 1. Включаем тактирование порта A, USART1 и контроллера DMA1
    *(volatile uint32_t *)0x40021018UL |= (1UL << 2) | (1UL << 14); // APB2ENR: IOPAEN, USART1EN
    RCC_AHBENR |= RCC_AHBENR_DMA1EN;

    // 2. Конфигурация пинов PA9 (TX - Alt PP) и PA10 (RX - Floating Input)
    uint32_t crh = *(volatile uint32_t *)0x40010804UL; // GPIOA_CRH
    crh &= ~0xFF0UL; // Очищаем настройки пинов 9 и 10
    crh |= 0x4B0UL;  // PA9: AF PP (0xB), PA10: Float Input (0x4)
    *(volatile uint32_t *)0x40010804UL = crh;

    // 3. Настройка USART1: Скорость 115200 при 8МГц HSI (BRR = 0x457)
    *(volatile uint32_t *)0x40013808UL = 0x457UL; // USART1_BRR

    // 4. Конфигурация DMA1 Channel 5 (Прием - RX)
    DMA1_CPAR5 = (uint32_t)&USART1_DR;
    DMA1_CMAR5 = (uint32_t)(uintptr_t)g_rx_dma_buffer;
    DMA1_CNDTR5 = UART_RX_DMA_BUF_SIZE;
    /* CCR5: 
       MINC (1<<7) - инкремент адреса памяти
       CIRC (1<<5) - циклический режим буфера
       EN   (1<<0) - запуск канала
    */
    DMA1_CCR5 = (1UL << 7) | (1UL << 5) | (1UL << 0);

    // 5. Конфигурация DMA1 Channel 4 (Передача - TX)
    DMA1_CPAR4 = (uint32_t)&USART1_DR;
    /* CCR4:
       DIR  (1<<4) - направление: из памяти в периферию
       MINC (1<<7) - инкремент адреса памяти
       TCIE (1<<1) - прерывание по окончании передачи
    */
    DMA1_CCR4 = (1UL << 4) | (1UL << 7) | (1UL << 1);

    // 6. Включаем аппаратный запрос DMA в USART1 и прерывания
    USART1_CR3 |= USART1_CR3_DMAT | USART1_CR3_DMAR;
    // Разрешаем работу UART, передатчика, приемника + прерывание по тишине линии (IDLEIE)
    USART1_CR1 = (1UL << 13) | (1UL << 3) | (1UL << 2) | USART1_CR1_IDLEIE;

    // 7. Разрешаем прерывания в NVIC
    NVIC_ISER0 |= DMA1_CH4_IRQ_BIT; // Прерывание завершения TX DMA
    NVIC_ISER1 |= USART1_IRQ_BIT;   // Прерывание IDLE Line в UART
}

/**
 * Асинхронный запуск передачи пачки данных через DMA
 */
bool uart1_tx_dma_async(const uint8_t *data, size_t len)
{
    // Если предыдущая передача DMA еще активна — канал включен, выходим
    if ((DMA1_CCR4 & 1UL) != 0u) {
        return false;
    }

    if (g_rs485_mode && g_gpio_bsrr_ptr != NULL) {
        *g_gpio_bsrr_ptr = g_de_pin_mask; // Переключаем MAX3485 на передачу (DE=1)
    }

    // Сбрасываем флаг TC (Transmission Complete) в UART, чтобы отследить физический уход байт
    USART1_SR &= ~(1UL << 6); 

    DMA1_CCR4 &= ~1UL; // Выключаем канал для переконфигурации
    DMA1_CMAR4 = (uint32_t)(uintptr_t)data;
    DMA1_CNDTR4 = len;
    DMA1_CCR4 |= 1UL;  // Запускаем передачу через DMA

    return true;
}

/**
 * ИСР: Прерывание DMA1 по окончании передачи (Channel 4)
 */
void DMA1_Channel4_IRQHandler(void)
{
    // Проверяем флаг завершения передачи (TCIF4) в регистре ISR
    if ((DMA1_ISR & (1UL << 13)) != 0u) {
        DMA1_IFCR = (1UL << 13); // Сбрасываем флаг прерывания
        DMA1_CCR4 &= ~1UL;       // Выключаем канал DMA

        // DMA выплюнул всё в регистр данных, но байты еще могут физически лететь по проводам.
        // Ждем флага TC (Transmission Complete) от самого UART.
        while ((USART1_SR & (1UL << 6)) == 0u) {
            // Короткое ожидание ухода последнего стоп-бита
        }

        if (g_rs485_mode && g_gpio_brr_ptr != NULL) {
            *g_gpio_brr_ptr = g_de_pin_mask; // Возвращаем MAX3485 на прием (RE=active)
        }

        if (g_uart_callback != NULL) {
            g_uart_callback(NULL, 0u, UART_EVENT_TX_COMPLETE);
        }
    }
}

/**
 * ИСР: Прерывание UART1 (Ловит событие тишины шины IDLE)
 */
void USART1_IRQHandler(void)
{
    uint32_t sr = USART1_SR;

    if ((sr & USART1_SR_IDLE) != 0u) {
        // Очистка флага IDLE в STM32F1: сначала читаем SR (уже сделано), затем читаем DR
        volatile uint32_t dummy = USART1_DR;
        (void)dummy;

        // Вычисляем, сколько байт сейчас лежит в кольцевом буфере DMA
        // CNDTR5 считает вниз от исходного размера буфера
        size_t current_dma_pos = UART_RX_DMA_BUF_SIZE - DMA1_CNDTR5;
        size_t len = 0u;

        if (current_dma_pos != g_last_rx_pos) {
            if (current_dma_pos > g_last_rx_pos) {
                len = current_dma_pos - g_last_rx_pos;
                if (g_uart_callback != NULL) {
                    // Передаем указатель на начало пачки БЕЗ копирования (Zero-copy)
                    g_uart_callback(&g_rx_dma_buffer[g_last_rx_pos], len, UART_EVENT_RX_CHUNK);
                }
            } else {
                // DMA закольцевался (произошел wrap-around)
                // В реальном Modbus RTU пакет не может быть разорван переходом через край 256-байтного буфера,
                // так как максимальный размер фрейма Modbus = 256 байт. 
                // Для закрытия гештальта обрабатываем этот редкий случай:
                len = UART_RX_DMA_BUF_SIZE - g_last_rx_pos;
                if (g_uart_callback != NULL && len > 0u) {
                    g_uart_callback(&g_rx_dma_buffer[g_last_rx_pos], len, UART_EVENT_RX_CHUNK);
                }
                if (g_uart_callback != NULL && current_dma_pos > 0u) {
                    g_uart_callback(&g_rx_dma_buffer[0], current_dma_pos, UART_EVENT_RX_CHUNK);
                }
            }
            g_last_rx_pos = current_dma_pos;
        }
    }
}
