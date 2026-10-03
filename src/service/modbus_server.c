#include "service/modbus_server.h"
#include "service/modbus.h"
#include "service/modbus_regs.h"
#include "bsp/stm32f1/uart.h"
#include <stddef.h>

// Внешняя функция обработки ответов (реализована в приложении)
extern void modbus_server_on_request_ready(const modbus_request_t *req);

// Локальный контекст для сохранения совместимости со старой структурой
static modbus_server_t *g_current_server_ctx = NULL;

/**
 * Асинхронный коллбэк обработки событий UART/DMA.
 * Вызывается напрямую из аппаратных прерываний.
 */
void modbus_uart_event_callback(const uint8_t *data, size_t len, uart_event_t event)
{
    if (g_current_server_ctx == NULL) {
        return;
    }

    if (event == UART_EVENT_ERROR) {
        g_current_server_ctx->error_count++;
        g_current_server_ctx->event = MODBUS_EVENT_CRC_ERROR;
        return;
    }

    if (event == UART_EVENT_TX_COMPLETE) {
        return;
    }

    if (event == UART_EVENT_RX_CHUNK) {
        g_current_server_ctx->rx_byte_count += len;
        
        // Zero-copy парсинг кадра напрямую из физического буфера DMA
        modbus_parse_err_t parse_err = modbus_parse_request(
            data, 
            len, 
            g_current_server_ctx->slave_addr, 
            &g_current_server_ctx->request
        );

        g_current_server_ctx->parse_error = parse_err;

        switch (parse_err) {
            case MODBUS_OK:
                g_current_server_ctx->frame_count++;
                g_current_server_ctx->event = MODBUS_EVENT_REQUEST;
                // Оповещаем архитектурный слой приложения
                modbus_server_on_request_ready(&g_current_server_ctx->request);
                break;
            case MODBUS_ERR_CRC:
                g_current_server_ctx->event = MODBUS_EVENT_CRC_ERROR;
                g_current_server_ctx->error_count++;
                break;
            case MODBUS_ERR_BAD_ADDR:
                g_current_server_ctx->event = MODBUS_EVENT_ADDRESS_ERROR;
                g_current_server_ctx->error_count++;
                break;
            case MODBUS_ERR_UNSUPPORTED_FUNC:
                g_current_server_ctx->event = MODBUS_EVENT_FUNCTION_ERROR;
                g_current_server_ctx->error_count++;
                break;
            default:
                g_current_server_ctx->event = MODBUS_EVENT_LENGTH_ERROR;
                g_current_server_ctx->error_count++;
                break;
        }
    }
}

/**
 * Инициализация сервера (сохранена совместимость со старыми тестами)
 */
void modbus_server_init(
    modbus_server_t *s,
    uint8_t slave_addr,
    uint32_t silence_ms,
    modbus_rx_byte_fn recv,
    void *recv_ctx)
{
    if (s == NULL) {
        return;
    }

    s->len = 0u;
    s->last_byte_ms = 0u;
    s->silence_ms = silence_ms;
    s->slave_addr = slave_addr;
    s->recv = recv;
    s->recv_ctx = recv_ctx;

    s->event = MODBUS_EVENT_NONE;
    s->request.slave_addr = 0u;
    s->request.function = 0u;
    s->request.data = NULL;
    s->request.data_len = 0u;
    s->parse_error = MODBUS_OK;

    s->rx_byte_count = 0u;
    s->frame_count = 0u;
    s->error_count = 0u;
    s->overflow_count = 0u;

    // Запоминаем контекст для работы прерываний DMA
    g_current_server_ctx = s;
}

/**
 * Перевыпуск синхронной задачи под асинхронные рельсы.
 * Забирает байты из старого буфера опроса и скармливает их DMA-коллбэку,
 * полностью сохраняя совместимость со старыми юнит-тестами.
 */
err_t modbus_server_task(modbus_server_t *s, uint32_t now_ms)
{
    if (s == NULL || s->recv == NULL) {
        return ERR_INVALID_ARG;
    }

    // Сохраняем указатель на текущий рабочий контекст сервера
    g_current_server_ctx = s;

    // Шаг 1. Выкачиваем байты через старый интерфейс побайтового опроса
    for (uint8_t i = 0u; i < 16u; ++i) {
        uint8_t byte = 0u;

        if (!s->recv(s->recv_ctx, &byte)) {
            break; // Буфер пуст, выходим из цикла приема
        }

        if (s->len >= MODBUS_SERVER_MAX_FRAME) {
            s->overflow_count++;
            s->error_count++;
            s->len = 0u;
            s->event = MODBUS_EVENT_OVERFLOW;
            return ERR_OK;
        }

        s->buf[s->len++] = byte;
        s->last_byte_ms = now_ms;
        s->rx_byte_count++;
    }

    // Шаг 2. Фиксируем межфреймовую тишину (эмуляция прерывания IDLE Line)
    if (s->len > 0u &&
        (int32_t)(now_ms - s->last_byte_ms) >= (int32_t)s->silence_ms) 
    {
        size_t current_frame_len = s->len;
        
        // Сбрасываем счетчик длины буфера ДО вызова коллбэка (профилактика гонки)
        s->len = 0u; 

        // Имитируем падение пачки данных из DMA-буфера прямо в наш асинхронный обработчик
        modbus_uart_event_callback(s->buf, current_frame_len, UART_EVENT_RX_CHUNK);
    }

    return ERR_OK;
}

void modbus_server_clear_event(modbus_server_t *s)
{
    if (s != NULL) {
        s->event = MODBUS_EVENT_NONE;
        s->parse_error = MODBUS_OK;
    }
}

const char *modbus_event_str(modbus_event_t e)
{
    switch (e) {
    case MODBUS_EVENT_NONE:           return "NONE";
    case MODBUS_EVENT_REQUEST:        return "REQUEST";
    case MODBUS_EVENT_CRC_ERROR:      return "CRC_ERROR";
    case MODBUS_EVENT_ADDRESS_ERROR:  return "ADDRESS_ERROR";
    case MODBUS_EVENT_FUNCTION_ERROR: return "FUNCTION_ERROR";
    case MODBUS_EVENT_LENGTH_ERROR:   return "LENGTH_ERROR";
    case MODBUS_EVENT_OVERFLOW:       return "OVERFLOW";
    default:                          return "UNKNOWN";
    }
}
