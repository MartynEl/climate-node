#include "service/modbus.h"

#include <stddef.h>
#include <stdint.h>

uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;

    if (len == 0u) {
        return crc;
    }

    if (data == NULL) {
        return 0u;
    }

    for (size_t i = 0u; i < len; ++i) {
        uint8_t byte = data[i];
        crc ^= (uint16_t)byte;

        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            if ((crc & 0x0001u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }

    return crc;
}

bool modbus_frame_has_valid_crc(const uint8_t *frame, size_t len)
{
    if (frame == NULL || len < 3u) {
        return false;
    }

    uint16_t calculated = modbus_crc16(frame, len - 2u);

    uint16_t received = (uint16_t)frame[len - 2u];
    received |= (uint16_t)((uint16_t)frame[len - 1u] << 8);

    return calculated == received;
}

modbus_parse_err_t modbus_parse_request(
    const uint8_t *frame,
    size_t len,
    uint8_t own_slave_addr,
    modbus_request_t *out)
{
    if (frame == NULL || out == NULL) {
        return MODBUS_ERR_INVALID_ARG;
    }

    if (len < 5u) {
        return MODBUS_ERR_SHORT_FRAME;
    }

    if (!modbus_frame_has_valid_crc(frame, len)) {
        return MODBUS_ERR_CRC;
    }

    uint8_t slave_addr = frame[0];
    uint8_t function = frame[1];

    if (slave_addr != own_slave_addr && slave_addr != 0u) {
        return MODBUS_ERR_BAD_ADDR;
    }

    size_t payload_len = len - 4u;

    out->slave_addr = slave_addr;
    out->function = function;
    out->data = &frame[2];
    out->data_len = payload_len;

    switch (function) {
    case 0x03u: /* Read Holding Registers */
    case 0x06u: /* Write Single Register */
        if (payload_len != 4u) {
            return MODBUS_ERR_BAD_LENGTH;
        }
        return MODBUS_OK;

    default:
        return MODBUS_ERR_UNSUPPORTED_FUNC;
    }
}

const char *modbus_parse_err_str(modbus_parse_err_t e)
{
    switch (e) {
    case MODBUS_OK:
        return "OK";
    case MODBUS_ERR_INVALID_ARG:
        return "INVALID_ARG";
    case MODBUS_ERR_SHORT_FRAME:
        return "SHORT_FRAME";
    case MODBUS_ERR_CRC:
        return "CRC";
    case MODBUS_ERR_BAD_ADDR:
        return "BAD_ADDR";
    case MODBUS_ERR_UNSUPPORTED_FUNC:
        return "UNSUPPORTED_FUNC";
    case MODBUS_ERR_BAD_LENGTH:
        return "BAD_LENGTH";
    default:
        return "UNKNOWN";
    }
}
