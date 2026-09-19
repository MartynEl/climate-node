#include "service/modbus_server.h"

#include <stddef.h>

static void process_frame(modbus_server_t *s)
{
    if (s == NULL) {
        return;
    }

    s->frame_count++;

    modbus_parse_err_t e = modbus_parse_request(
        s->buf,
        s->len,
        s->slave_addr,
        &s->request);

    s->parse_error = e;

    switch (e) {
    case MODBUS_OK:
        s->event = MODBUS_EVENT_REQUEST;
        break;

    case MODBUS_ERR_CRC:
        s->event = MODBUS_EVENT_CRC_ERROR;
        s->error_count++;
        break;

    case MODBUS_ERR_BAD_ADDR:
        s->event = MODBUS_EVENT_ADDRESS_ERROR;
        s->error_count++;
        break;

    case MODBUS_ERR_UNSUPPORTED_FUNC:
        s->event = MODBUS_EVENT_FUNCTION_ERROR;
        s->error_count++;
        break;

    case MODBUS_ERR_SHORT_FRAME:
    case MODBUS_ERR_BAD_LENGTH:
        s->event = MODBUS_EVENT_LENGTH_ERROR;
        s->error_count++;
        break;

    case MODBUS_ERR_INVALID_ARG:
    default:
        s->event = MODBUS_EVENT_LENGTH_ERROR;
        s->error_count++;
        break;
    }

    s->len = 0u;
}

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

    for (size_t i = 0u; i < MODBUS_SERVER_MAX_FRAME; ++i) {
        s->buf[i] = 0u;
    }

    s->len = 0u;
    s->last_byte_ms = 0u;
    s->silence_ms = (silence_ms != 0u) ? silence_ms : MODBUS_SERVER_DEFAULT_SILENCE_MS;
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
}

err_t modbus_server_task(modbus_server_t *s, uint32_t now_ms)
{
    if (s == NULL || s->recv == NULL) {
        return ERR_INVALID_ARG;
    }

    /*
     * Drain a bounded number of bytes per call.
     * This keeps the task non-blocking even if the ring buffer has many bytes.
     */
    for (uint8_t i = 0u; i < 16u; ++i) {
        uint8_t byte = 0u;

        if (!s->recv(s->recv_ctx, &byte)) {
            break;
        }

        if (s->len >= MODBUS_SERVER_MAX_FRAME) {
            s->overflow_count++;
            s->error_count++;
            s->len = 0u;
            s->event = MODBUS_EVENT_OVERFLOW;
        }

        s->buf[s->len++] = byte;
        s->last_byte_ms = now_ms;
        s->rx_byte_count++;
    }

    if (s->len > 0u &&
        (int32_t)(now_ms - s->last_byte_ms) >= (int32_t)s->silence_ms) {
        process_frame(s);
    }

    return ERR_OK;
}

void modbus_server_clear_event(modbus_server_t *s)
{
    if (s == NULL) {
        return;
    }

    s->event = MODBUS_EVENT_NONE;
    s->parse_error = MODBUS_OK;
}

const char *modbus_event_str(modbus_event_t e)
{
    switch (e) {
    case MODBUS_EVENT_NONE:
        return "NONE";
    case MODBUS_EVENT_REQUEST:
        return "REQUEST";
    case MODBUS_EVENT_CRC_ERROR:
        return "CRC_ERROR";
    case MODBUS_EVENT_ADDRESS_ERROR:
        return "ADDRESS_ERROR";
    case MODBUS_EVENT_FUNCTION_ERROR:
        return "FUNCTION_ERROR";
    case MODBUS_EVENT_LENGTH_ERROR:
        return "LENGTH_ERROR";
    case MODBUS_EVENT_OVERFLOW:
        return "OVERFLOW";
    default:
        return "UNKNOWN";
    }
}
