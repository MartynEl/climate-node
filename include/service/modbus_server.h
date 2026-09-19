#ifndef SERVICE_MODBUS_SERVER_H
#define SERVICE_MODBUS_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/errors.h"
#include "service/modbus.h"

enum {
    MODBUS_SERVER_MAX_FRAME = 256
};

enum {
    MODBUS_SERVER_DEFAULT_SILENCE_MS = 5
};

typedef bool (*modbus_rx_byte_fn)(void *ctx, uint8_t *out);

typedef enum {
    MODBUS_EVENT_NONE = 0,
    MODBUS_EVENT_REQUEST,
    MODBUS_EVENT_CRC_ERROR,
    MODBUS_EVENT_ADDRESS_ERROR,
    MODBUS_EVENT_FUNCTION_ERROR,
    MODBUS_EVENT_LENGTH_ERROR,
    MODBUS_EVENT_OVERFLOW
} modbus_event_t;

typedef struct {
    uint8_t buf[MODBUS_SERVER_MAX_FRAME];
    size_t len;

    uint32_t last_byte_ms;
    uint32_t silence_ms;
    uint8_t slave_addr;

    modbus_rx_byte_fn recv;
    void *recv_ctx;

    modbus_event_t event;
    modbus_request_t request;
    modbus_parse_err_t parse_error;

    uint32_t rx_byte_count;
    uint32_t frame_count;
    uint32_t error_count;
    uint32_t overflow_count;
} modbus_server_t;

void modbus_server_init(
    modbus_server_t *s,
    uint8_t slave_addr,
    uint32_t silence_ms,
    modbus_rx_byte_fn recv,
    void *recv_ctx);

err_t modbus_server_task(modbus_server_t *s, uint32_t now_ms);

void modbus_server_clear_event(modbus_server_t *s);

const char *modbus_event_str(modbus_event_t e);

#endif // SERVICE_MODBUS_SERVER_H
