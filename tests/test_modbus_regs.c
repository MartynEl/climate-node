#include "service/modbus.h"
#include "service/modbus_regs.h"
#include "test_support.h"

static void test_init_defaults(void)
{
    modbus_reg_map_t regs;
    mb_regs_init(&regs);

    CHECK_EQ_INT(regs.cfg_margin_on_cd, 500);
    CHECK_EQ_INT(regs.cfg_margin_off_cd, 700);
    CHECK_EQ_INT(regs.cfg_sample_ms, 1000);
    CHECK_EQ_INT(regs.temp_filt_cd, 0);
}

static void test_read_valid_range(void)
{
    modbus_reg_map_t regs;
    mb_regs_init(&regs);

    regs.temp_filt_cd = 2250; /* 22.50 C */
    regs.rh_cp = 6500;        /* 65.00 % */
    regs.dew_point_cd = 1560; /* 15.60 C */
    regs.relay_on = true;     /* 1 */

    uint8_t buf[10];
    
    /* 
     * Читаем 4 регистра:
     * 0x0000 Temp
     * 0x0001 RH
     * 0x0002 Dew Point
     * 0x0003 Relay
     */
    bool ok = mb_regs_read(&regs, 0, 4, buf, sizeof(buf));

    CHECK_TRUE(ok);
    
    /* Reg 0 (Temp): 2250 -> 0x08CA -> BE: 08 CA */
    CHECK_EQ_INT(buf[0], 0x08);
    CHECK_EQ_INT(buf[1], 0xCA);

    /* Reg 1 (RH): 6500 -> 0x1964 -> BE: 19 64 */
    CHECK_EQ_INT(buf[2], 0x19);
    CHECK_EQ_INT(buf[3], 0x64);

    /* Reg 2 (Dew): 1560 -> 0x0618 -> BE: 06 18 */
    CHECK_EQ_INT(buf[4], 0x06);
    CHECK_EQ_INT(buf[5], 0x18);

    /* Reg 3 (Relay): 1 -> 0x0001 -> BE: 00 01 */
    CHECK_EQ_INT(buf[6], 0x00);
    CHECK_EQ_INT(buf[7], 0x01);
}

static void test_read_out_of_bounds(void)
{
    modbus_reg_map_t regs;
    mb_regs_init(&regs);

    uint8_t buf[10];
    /* Try to read past defined range */
    bool ok = mb_regs_read(&regs, 0x0200, 1, buf, sizeof(buf));

    CHECK_FALSE(ok);
}

static void test_write_config(void)
{
    modbus_reg_map_t regs;
    mb_regs_init(&regs);

    bool ok = mb_regs_write_single(&regs, MB_CFG_MARGIN_ON, 600);
    CHECK_TRUE(ok);
    CHECK_EQ_INT(regs.cfg_margin_on_cd, 600);

    ok = mb_regs_write_single(&regs, MB_CFG_SAMPLE_MS, 500);
    CHECK_TRUE(ok);
    CHECK_EQ_INT(regs.cfg_sample_ms, 500);

    /* Invalid: Zero sample period */
    ok = mb_regs_write_single(&regs, MB_CFG_SAMPLE_MS, 0);
    CHECK_FALSE(ok);
}

static void test_builder_response(void)
{
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t frame[20];

    size_t len = modbus_build_response(1, 3, payload, 4, frame, sizeof(frame));

    CHECK_EQ_INT(len, 8); /* 1(addr)+1(func)+4(payload)+2(crc) */
    CHECK_EQ_INT(frame[0], 1);
    CHECK_EQ_INT(frame[1], 3);
    CHECK_EQ_INT(frame[2], 1);
    CHECK_EQ_INT(frame[3], 2);
    CHECK_EQ_INT(frame[4], 3);
    CHECK_EQ_INT(frame[5], 4);

    /* Verify CRC */
    CHECK_TRUE(modbus_frame_has_valid_crc(frame, len));
}

static void test_builder_exception(void)
{
    uint8_t frame[10];
    size_t len = modbus_build_exception(1, 3, 2, frame, sizeof(frame));

    CHECK_EQ_INT(len, 5);
    CHECK_EQ_INT(frame[0], 1);
    CHECK_EQ_INT(frame[1], 0x83); /* 3 | 0x80 */
    CHECK_EQ_INT(frame[2], 2);

    CHECK_TRUE(modbus_frame_has_valid_crc(frame, len));
}

int main(void)
{
    RUN(test_init_defaults);
    RUN(test_read_valid_range);
    RUN(test_read_out_of_bounds);
    RUN(test_write_config);
    RUN(test_builder_response);
    RUN(test_builder_exception);

    return test_report();
}
