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

size_t modbus_build_response(
    uint8_t slave_addr,
    uint8_t function,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *out_buf,
    size_t out_cap)
{
    if (out_buf == NULL || out_cap < 5u) {
        return 0u;
    }

    /* Header: Addr + Func */
    out_buf[0] = slave_addr;
    out_buf[1] = function;

    /* Payload */
    if (payload != NULL && payload_len > 0u) {
        if (2u + payload_len + 2u > out_cap) {
            return 0u;
        }
        for (size_t i = 0u; i < payload_len; ++i) {
            out_buf[2u + i] = payload[i];
        }
    } else {
        if (2u + 2u > out_cap) {
            return 0u;
        }
    }

    /* CRC over everything except last 2 bytes */
    size_t total_len_no_crc = 2u + payload_len;
    uint16_t crc = modbus_crc16(out_buf, total_len_no_crc);

    out_buf[total_len_no_crc] = (uint8_t)(crc & 0xFFu);
    out_buf[total_len_no_crc + 1u] = (uint8_t)(crc >> 8);

    return total_len_no_crc + 2u;
}

size_t modbus_build_exception(
    uint8_t slave_addr,
    uint8_t original_func,
    uint8_t exception_code,
    uint8_t *out_buf,
    size_t out_cap)
{
    if (out_buf == NULL || out_cap < 5u) {
        return 0u;
    }

    out_buf[0] = slave_addr;
    out_buf[1] = original_func | 0x80u; /* Set high bit for exception */
    out_buf[2] = exception_code;

    uint16_t crc = modbus_crc16(out_buf, 3u);

    out_buf[3] = (uint8_t)(crc & 0xFFu);
    out_buf[4] = (uint8_t)(crc >> 8);

    return 5u;
}
