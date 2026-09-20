#include "service/storage.h"
#include "test_support.h"

static void test_defaults_valid(void)
{
    device_config_t cfg;
    storage_load_defaults(&cfg);

    CHECK_TRUE(storage_validate(&cfg));
    CHECK_EQ_INT(cfg.dew_margin_on_cd, 500);
}

static void test_corruption_detected(void)
{
    device_config_t cfg;
    storage_load_defaults(&cfg);

    cfg.dew_margin_on_cd = 999; /* Change data */
    /* Do NOT update CRC */

    CHECK_FALSE(storage_validate(&cfg));
}

static void test_finalize_fixes_crc(void)
{
    device_config_t cfg;
    storage_load_defaults(&cfg);

    cfg.dew_margin_on_cd = 999;
    CHECK_FALSE(storage_validate(&cfg));

    storage_finalize(&cfg);
    CHECK_TRUE(storage_validate(&cfg));
    CHECK_EQ_INT(cfg.dew_margin_on_cd, 999);
}

int main(void)
{
    RUN(test_defaults_valid);
    RUN(test_corruption_detected);
    RUN(test_finalize_fixes_crc);

    return test_report();
}
