#include "driver/sensor_sht31.h"
#include "bsp/i2c.h"
#include "test_support.h"

#include <string.h>

/*
 * Mock I2C state controlled by test cases.
 */
static uint8_t g_mock_i2c_data[6];
static bool g_mock_i2c_fail = false;

/* Strong override for testing only (no attribute needed, default is strong) */
err_t i2c_init(void)
{
    return ERR_OK;
}

err_t i2c_mem_write(
    uint8_t dev_addr,
    uint8_t reg_addr,
    const uint8_t *data,
    size_t len)
{
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    return g_mock_i2c_fail ? ERR_COMM : ERR_OK;
}

err_t i2c_mem_read(
    uint8_t dev_addr,
    uint8_t reg_addr,
    uint8_t *data,
    size_t len)
{
    (void)dev_addr;
    (void)reg_addr;

    if (g_mock_i2c_fail) {
        return ERR_COMM;
    }

    if (len >= 6) {
        memcpy(data, g_mock_i2c_data, 6);
    }
    return ERR_OK;
}

static void set_mock_values(uint16_t st_raw, uint16_t srh_raw)
{
    g_mock_i2c_data[0] = (uint8_t)(st_raw >> 8);
    g_mock_i2c_data[1] = (uint8_t)(st_raw & 0xFF);
    g_mock_i2c_data[2] = 0xBE; /* Dummy CRC */

    g_mock_i2c_data[3] = (uint8_t)(srh_raw >> 8);
    g_mock_i2c_data[4] = (uint8_t)(srh_raw & 0xFF);
    g_mock_i2c_data[5] = 0xEF; /* Dummy CRC */

    g_mock_i2c_fail = false;
}

static void test_conversion_22_5c_65pct(void)
{
    set_mock_values(25277, 42597);

    sample_t s;
    err_t e = sht31_read_sample(NULL, &s);

    CHECK_EQ_INT(e, ERR_OK);
    CHECK_EQ_INT(s.quality, QUALITY_VALID);

    CHECK(s.temp_cd >= 2240 && s.temp_cd <= 2260);
    CHECK(s.rh_cp >= 6490 && s.rh_cp <= 6510);
}

static void test_communication_failure(void)
{
    g_mock_i2c_fail = true;

    sample_t s;
    err_t e = sht31_read_sample(NULL, &s);

    CHECK_EQ_INT(e, ERR_COMM);
    CHECK_EQ_INT(s.quality, QUALITY_FAULT);
}

static void test_out_of_range_temperature(void)
{
    set_mock_values(0, 42597);

    sample_t s;
    err_t e = sht31_read_sample(NULL, &s);

    CHECK_EQ_INT(e, ERR_OUT_OF_RANGE);
    CHECK_EQ_INT(s.quality, QUALITY_FAULT);
}

int main(void)
{
    RUN(test_conversion_22_5c_65pct);
    RUN(test_communication_failure);
    RUN(test_out_of_range_temperature);

    return test_report();
}
