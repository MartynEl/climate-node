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

/* Response builders */

/**
 * Builds a standard Modbus RTU response frame.
 * 
 * @param slave_addr Target slave address.
 * @param function Function code (e.g., 0x03, 0x06).
 * @param payload Pointer to payload bytes (after header, before CRC).
 * @param payload_len Length of payload.
 * @param out_buf Buffer to write full frame (addr + func + payload + crc).
 * @param out_cap Capacity of out_buf.
 * @return Number of bytes written, or 0 on error.
 */
size_t modbus_build_response(
    uint8_t slave_addr,
    uint8_t function,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t *out_buf,
    size_t out_cap);

/**
 * Builds a Modbus Exception Response.
 * 
 * @param slave_addr Target slave address.
 * @param original_func Original function code that caused error.
 * @param exception_code Standard Modbus exception code (1..4).
 * @param out_buf Buffer to write frame.
 * @param out_cap Capacity.
 * @return Bytes written.
 */
size_t modbus_build_exception(
    uint8_t slave_addr,
    uint8_t original_func,
    uint8_t exception_code,
    uint8_t *out_buf,
    size_t out_cap);

/* Common exception codes */
#define MODBUS_EX_ILLEGAL_FUNC      0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDR 0x02
#define MODBUS_EX_ILLEGAL_DATA_VAL  0x03
#define MODBUS_EX_SLAVE_DEVICE_FAIL 0x04

