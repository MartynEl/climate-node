#include "test_support.h"

#include "core/dewpoint.h"

static int32_t abs32(int32_t x)
{
    return x < 0 ? -x : x;
}

static void test_known_values(void)
{
    temp_cd_t dew = 0;

    /*
     * 20.00 degC, 50.00 % RH.
     * Expected dew point is approximately 9.26 degC.
     */
    err_t e = dewpoint_calc_cd(2000, 5000, &dew);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK(dew > 850 && dew < 1000);

    /*
     * 22.50 degC, 65.00 % RH.
     * Expected dew point is approximately 15.6 degC.
     */
    e = dewpoint_calc_cd(2250, 6500, &dew);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK(dew > 1500 && dew < 1620);
}

static void test_saturation(void)
{
    temp_cd_t dew = 0;

    /* At 100% RH dew point should be very close to air temperature. */
    err_t e = dewpoint_calc_cd(2250, 10000, &dew);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK(abs32(dew - 2250) <= 30);
}

static void test_clamping(void)
{
    temp_cd_t dew = 0;

    /* Humidity above 100% should be clamped, not rejected. */
    err_t e = dewpoint_calc_cd(2250, 12000, &dew);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK(dew <= 2250);

    /* Humidity below 1% is clamped to 1% in this implementation. */
    e = dewpoint_calc_cd(2250, 0, &dew);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK(dew <= 2250);
}

static void test_invalid_temperature(void)
{
    temp_cd_t dew = 0;

    CHECK_EQ_INT(dewpoint_calc_cd(-5000, 5000, &dew), ERR_OUT_OF_RANGE);
    CHECK_EQ_INT(dewpoint_calc_cd(9000, 5000, &dew), ERR_OUT_OF_RANGE);
}

static void test_null_output(void)
{
    CHECK_EQ_INT(dewpoint_calc_cd(2000, 5000, NULL), ERR_INVALID_ARG);
}

int main(void)
{
    RUN(test_known_values);
    RUN(test_saturation);
    RUN(test_clamping);
    RUN(test_invalid_temperature);
    RUN(test_null_output);

    return test_report();
}