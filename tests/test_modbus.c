#include "service/modbus.h"
#include "test_support.h"

static void append_crc(uint8_t *buf, size_t payload_and_header_len)
{
    uint16_t crc = modbus_crc16(buf, payload_and_header_len);
    buf[payload_and_header_len] = (uint8_t)(crc & 0xFFu);
    buf[payload_and_header_len + 1u] = (uint8_t)(crc >> 8);
}

static void test_crc_known_vector(void)
{
    const uint8_t data[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    uint16_t crc = modbus_crc16(data, sizeof(data));

    CHECK_EQ_INT(crc, 0x4B37);
}

static void test_crc_empty(void)
{
    CHECK_EQ_INT(modbus_crc16(NULL, 0u), 0xFFFF);
}

static void test_valid_read_holding_registers(void)
{
    uint8_t frame[8] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_OK);
    CHECK_EQ_INT(req.slave_addr, 1u);
    CHECK_EQ_INT(req.function, 3u);
    CHECK_EQ_INT(req.data_len, 4u);
    CHECK_EQ_INT(req.data[0], 0x00u);
    CHECK_EQ_INT(req.data[1], 0x00u);
    CHECK_EQ_INT(req.data[2], 0x00u);
    CHECK_EQ_INT(req.data[3], 0x01u);
}

static void test_valid_write_single_register(void)
{
    uint8_t frame[8] = {
        0x01u, 0x06u, 0x00u, 0x10u, 0x00u, 0x2Au, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_OK);
    CHECK_EQ_INT(req.function, 6u);
    CHECK_EQ_INT(req.data_len, 4u);
}

static void test_bad_crc(void)
{
    uint8_t frame[8] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    frame[7u] ^= 0x55u;

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_ERR_CRC);
}

static void test_short_frame(void)
{
    uint8_t frame[4] = {0x01u, 0x03u, 0x00u, 0x00u};

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_ERR_SHORT_FRAME);
}

static void test_wrong_slave_address(void)
{
    uint8_t frame[8] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 2u, &req);

    CHECK_EQ_INT(e, MODBUS_ERR_BAD_ADDR);
}

static void test_broadcast_address_accepted(void)
{
    uint8_t frame[8] = {
        0x00u, 0x06u, 0x00u, 0x10u, 0x00u, 0x2Au, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_OK);
    CHECK_EQ_INT(req.slave_addr, 0u);
}

static void test_unsupported_function(void)
{
    uint8_t frame[8] = {
        0x01u, 0x07u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_ERR_UNSUPPORTED_FUNC);
}

static void test_bad_payload_length(void)
{
    uint8_t frame[7] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };

    append_crc(frame, 5u);

    modbus_request_t req;
    modbus_parse_err_t e = modbus_parse_request(frame, sizeof(frame), 1u, &req);

    CHECK_EQ_INT(e, MODBUS_ERR_BAD_LENGTH);
}

static void test_null_args(void)
{
    uint8_t frame[8] = {0};
    modbus_request_t req;

    CHECK_EQ_INT(modbus_parse_request(NULL, sizeof(frame), 1u, &req), MODBUS_ERR_INVALID_ARG);
    CHECK_EQ_INT(modbus_parse_request(frame, sizeof(frame), 1u, NULL), MODBUS_ERR_INVALID_ARG);
}

int main(void)
{
    RUN(test_crc_known_vector);
    RUN(test_crc_empty);
    RUN(test_valid_read_holding_registers);
    RUN(test_valid_write_single_register);
    RUN(test_bad_crc);
    RUN(test_short_frame);
    RUN(test_wrong_slave_address);
    RUN(test_broadcast_address_accepted);
    RUN(test_unsupported_function);
    RUN(test_bad_payload_length);
    RUN(test_null_args);

    return test_report();
}
