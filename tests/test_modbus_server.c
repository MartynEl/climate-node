#include "core/errors.h"
#include "service/modbus.h"
#include "service/modbus_server.h"
#include "test_support.h"

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t idx;
} fake_rx_t;

static bool fake_recv(void *ctx, uint8_t *out)
{
    fake_rx_t *f = (fake_rx_t *)ctx;

    if (f == NULL || out == NULL || f->data == NULL) {
        return false;
    }

    if (f->idx >= f->len) {
        return false;
    }

    *out = f->data[f->idx++];
    return true;
}

static void append_crc(uint8_t *buf, size_t payload_and_header_len)
{
    uint16_t crc = modbus_crc16(buf, payload_and_header_len);
    buf[payload_and_header_len] = (uint8_t)(crc & 0xFFu);
    buf[payload_and_header_len + 1u] = (uint8_t)(crc >> 8);
}

static void test_valid_request_frame(void)
{
    uint8_t frame[8] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    fake_rx_t rx = {
        .data = frame,
        .len = sizeof(frame),
        .idx = 0u
    };

    modbus_server_t s;
    modbus_server_init(&s, 1u, 5u, fake_recv, &rx);

    CHECK_EQ_INT(modbus_server_task(&s, 0u), ERR_OK);
    CHECK_EQ_INT(s.event, MODBUS_EVENT_NONE);

    CHECK_EQ_INT(modbus_server_task(&s, 4u), ERR_OK);
    CHECK_EQ_INT(s.event, MODBUS_EVENT_NONE);

    CHECK_EQ_INT(modbus_server_task(&s, 5u), ERR_OK);
    CHECK_EQ_INT(s.event, MODBUS_EVENT_REQUEST);
    CHECK_EQ_INT(s.request.slave_addr, 1u);
    CHECK_EQ_INT(s.request.function, 3u);
    CHECK_EQ_INT(s.request.data_len, 4u);

    modbus_server_clear_event(&s);
    CHECK_EQ_INT(s.event, MODBUS_EVENT_NONE);
}

static void test_crc_error(void)
{
    uint8_t frame[8] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);
    frame[7u] ^= 0x55u;

    fake_rx_t rx = {
        .data = frame,
        .len = sizeof(frame),
        .idx = 0u
    };

    modbus_server_t s;
    modbus_server_init(&s, 1u, 5u, fake_recv, &rx);

    (void)modbus_server_task(&s, 0u);
    (void)modbus_server_task(&s, 5u);

    CHECK_EQ_INT(s.event, MODBUS_EVENT_CRC_ERROR);
    CHECK_EQ_INT(s.parse_error, MODBUS_ERR_CRC);
}

static void test_address_error(void)
{
    uint8_t frame[8] = {
        0x01u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    fake_rx_t rx = {
        .data = frame,
        .len = sizeof(frame),
        .idx = 0u
    };

    modbus_server_t s;
    modbus_server_init(&s, 2u, 5u, fake_recv, &rx);

    (void)modbus_server_task(&s, 0u);
    (void)modbus_server_task(&s, 5u);

    CHECK_EQ_INT(s.event, MODBUS_EVENT_ADDRESS_ERROR);
    CHECK_EQ_INT(s.parse_error, MODBUS_ERR_BAD_ADDR);
}

static void test_unsupported_function(void)
{
    uint8_t frame[8] = {
        0x01u, 0x07u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    fake_rx_t rx = {
        .data = frame,
        .len = sizeof(frame),
        .idx = 0u
    };

    modbus_server_t s;
    modbus_server_init(&s, 1u, 5u, fake_recv, &rx);

    (void)modbus_server_task(&s, 0u);
    (void)modbus_server_task(&s, 5u);

    CHECK_EQ_INT(s.event, MODBUS_EVENT_FUNCTION_ERROR);
    CHECK_EQ_INT(s.parse_error, MODBUS_ERR_UNSUPPORTED_FUNC);
}

static void test_broadcast_accepted(void)
{
    uint8_t frame[8] = {
        0x00u, 0x06u, 0x00u, 0x10u, 0x00u, 0x2Au, 0x00u, 0x00u
    };

    append_crc(frame, 6u);

    fake_rx_t rx = {
        .data = frame,
        .len = sizeof(frame),
        .idx = 0u
    };

    modbus_server_t s;
    modbus_server_init(&s, 1u, 5u, fake_recv, &rx);

    (void)modbus_server_task(&s, 0u);
    (void)modbus_server_task(&s, 5u);

    CHECK_EQ_INT(s.event, MODBUS_EVENT_REQUEST);
    CHECK_EQ_INT(s.request.slave_addr, 0u);
}

int main(void)
{
    RUN(test_valid_request_frame);
    RUN(test_crc_error);
    RUN(test_address_error);
    RUN(test_unsupported_function);
    RUN(test_broadcast_accepted);

    return test_report();
}
