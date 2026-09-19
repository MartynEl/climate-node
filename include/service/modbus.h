#ifndef SERVICE_MODBUS_H
#define SERVICE_MODBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MODBUS_OK = 0,
    MODBUS_ERR_INVALID_ARG,
    MODBUS_ERR_SHORT_FRAME,
    MODBUS_ERR_CRC,
    MODBUS_ERR_BAD_ADDR,
    MODBUS_ERR_UNSUPPORTED_FUNC,
    MODBUS_ERR_BAD_LENGTH
} modbus_parse_err_t;

typedef struct {
    uint8_t slave_addr;
    uint8_t function;
    const uint8_t *data;
    size_t data_len;
} modbus_request_t;

uint16_t modbus_crc16(const uint8_t *data, size_t len);

bool modbus_frame_has_valid_crc(const uint8_t *frame, size_t len);

modbus_parse_err_t modbus_parse_request(
    const uint8_t *frame,
    size_t len,
    uint8_t own_slave_addr,
    modbus_request_t *out);

const char *modbus_parse_err_str(modbus_parse_err_t e);

#endif // SERVICE_MODBUS_H
