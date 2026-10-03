#ifndef BSP_STM32F1_UART_H
#define BSP_STM32F1_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Типы событий асинхронного интерфейса связи
 */
typedef enum {
    UART_EVENT_RX_CHUNK,   // DMA заполнил часть буфера (HT/TC) или сработал таймаут IDLE
    UART_EVENT_TX_COMPLETE, // DMA полностью передал массив данных в линию
    UART_EVENT_ERROR        // Аппаратная ошибка (Overrun, Noise, Framing)
} uart_event_t;

/**
 * Сигнатура функции обратного вызова (Callback) для обработки событий UART/DMA.
 * @param data Указатель на начало пачки данных в памяти (ноль-копирование)
 * @param len Количество доступных байт
 * @param event Тип произошедшего события
 */
typedef void (*uart_callback_fn)(const uint8_t *data, size_t len, uart_event_t event);

/**
 * Инициализация UART1 и привязка асинхронных событий к верхнему слою.
 */
void uart1_async_init(uart_callback_fn callback);

/**
 * Асинхронный запуск передачи пачки данных через DMA.
 * В отличие от старого uart1_tx_bytes, эта функция не копирует данные в кольцевой
 * буфер драйвера. Она просто скармливает указатель на ваш Modbus-ответ контроллеру DMA.
 * 
 * Внимание: Буфер данных должен быть валиден в памяти до события UART_EVENT_TX_COMPLETE!
 */
bool uart1_tx_dma_async(const uint8_t *data, size_t len);

/**
 * Управление линией RS-485 DE/RE
 */
void uart1_set_rs485_pin(uint32_t gpio_base, uint32_t pin_mask);

#endif // BSP_STM32F1_UART_H
