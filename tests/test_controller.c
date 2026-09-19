#include "test_support.h"

#include "core/controller.h"

static sample_t make_sample(temp_cd_t t, rh_cp_t rh, quality_t q)
{
    sample_t s;
    s.temp_cd = t;
    s.rh_cp = rh;
    s.timestamp_ms = 0;
    s.quality = q;
    return s;
}

static void test_initial_safe_state(void)
{
    controller_t c;
    ctrl_init(&c, NULL);

    CHECK_EQ_INT(ctrl_get_state(&c), CTRL_STATE_INIT);
    CHECK_FALSE(ctrl_relay_on(&c));
}

static void test_sensor_fault_debounce(void)
{
    controller_t c;
    ctrl_init(&c, NULL);

    sample_t bad = make_sample(-9990, 6500, QUALITY_INVALID);

    err_t e = ctrl_update(&c, &bad);
    CHECK_EQ_INT(e, ERR_SENSOR);
    CHECK_FALSE(ctrl_relay_on(&c));

    e = ctrl_update(&c, &bad);
    CHECK_EQ_INT(e, ERR_SENSOR);
    CHECK_FALSE(ctrl_relay_on(&c));

    e = ctrl_update(&c, &bad);
    CHECK_EQ_INT(e, ERR_SENSOR);
    CHECK_EQ_INT(ctrl_get_state(&c), CTRL_STATE_SENSOR_FAULT);
    CHECK_FALSE(ctrl_relay_on(&c));
}

static void test_warmup_state(void)
{
    controller_t c;
    ctrl_init(&c, NULL);

    sample_t ok = make_sample(2250, 5000, QUALITY_VALID);

    for (int i = 0; i < MA_FILTER_SIZE - 1; ++i) {
        err_t e = ctrl_update(&c, &ok);
        CHECK_EQ_INT(e, ERR_WARMUP);
        CHECK_EQ_INT(ctrl_get_state(&c), CTRL_STATE_WARMUP);
        CHECK_FALSE(ctrl_relay_on(&c));
    }

    err_t e = ctrl_update(&c, &ok);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK_EQ_INT(ctrl_get_state(&c), CTRL_STATE_RUN);
}

static void test_hysteresis_on_off(void)
{
    controller_t c;
    ctrl_init(&c, NULL);

    /*
     * Cold-ish and very humid:
     * 20.00 degC, 95.00 % RH -> dew point near 19.2 degC.
     * Margin is below 5 degC, relay should turn on.
     */
    sample_t humid = make_sample(2000, 9500, QUALITY_VALID);

    for (int i = 0; i < MA_FILTER_SIZE; ++i) {
        err_t e = ctrl_update(&c, &humid);
        if (i < MA_FILTER_SIZE - 1) {
            CHECK_EQ_INT(e, ERR_WARMUP);
        } else {
            CHECK_EQ_INT(e, ERR_OK);
        }
    }

    CHECK_TRUE(ctrl_relay_on(&c));

    /*
     * Hotter and drier:
     * 30.00 degC, 50.00 % RH -> dew point near 18.4 degC.
     * Margin is above 7 degC, relay should eventually turn off.
     */
    sample_t dry = make_sample(3000, 5000, QUALITY_VALID);

    for (int i = 0; i < MA_FILTER_SIZE; ++i) {
        err_t e = ctrl_update(&c, &dry);
        CHECK_EQ_INT(e, ERR_OK);
    }

    CHECK_FALSE(ctrl_relay_on(&c));
}

static void test_null_args(void)
{
    controller_t c;
    ctrl_init(&c, NULL);

    sample_t s = make_sample(2250, 5000, QUALITY_VALID);

    CHECK_EQ_INT(ctrl_update(NULL, &s), ERR_INVALID_ARG);
    CHECK_EQ_INT(ctrl_update(&c, NULL), ERR_INVALID_ARG);
}

int main(void)
{
    RUN(test_initial_safe_state);
    RUN(test_sensor_fault_debounce);
    RUN(test_warmup_state);
    RUN(test_hysteresis_on_off);
    RUN(test_null_args);

    return test_report();
}