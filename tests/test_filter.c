#include "test_support.h"

#include "core/filter.h"

static void test_warmup(void)
{
    ma_filter_t f;
    ma_init(&f);

    int32_t avg = 0;
    uint8_t count = 0;

    err_t e = ma_update(&f, 2250, &avg, &count);

    CHECK_EQ_INT(e, ERR_WARMUP);
    CHECK_EQ_INT(avg, 2250);
    CHECK_EQ_INT(count, 1);

    e = ma_update(&f, 2250, &avg, &count);
    CHECK_EQ_INT(e, ERR_WARMUP);
    CHECK_EQ_INT(count, 2);

    e = ma_update(&f, 2250, &avg, &count);
    CHECK_EQ_INT(e, ERR_WARMUP);
    CHECK_EQ_INT(count, 3);

    e = ma_update(&f, 2250, &avg, &count);
    CHECK_EQ_INT(e, ERR_WARMUP);
    CHECK_EQ_INT(count, 4);

    e = ma_update(&f, 2250, &avg, &count);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK_EQ_INT(count, 5);
    CHECK_EQ_INT(avg, 2250);
}

static void test_running_average(void)
{
    ma_filter_t f;
    ma_init(&f);

    int32_t avg = 0;
    err_t e;

    /*
     * Warm-up is explicit.
     *
     * Even if input values are valid, the filter is not ready for control
     * until it has collected MA_FILTER_SIZE samples.
     */
    for (int i = 0; i < MA_FILTER_SIZE - 1; ++i) {
        e = ma_update(&f, 0, &avg, NULL);
        CHECK_EQ_INT(e, ERR_WARMUP);
    }

    /*
     * The last warm-up sample makes the filter ready.
     */
    e = ma_update(&f, 0, &avg, NULL);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK_EQ_INT(avg, 0);

    /*
     * Step input:
     *
     * Buffer is full of zeros:
     *   [0, 0, 0, 0, 0]
     *
     * After one sample 1000:
     *   [1000, 0, 0, 0, 0]
     *
     * Average:
     *   1000 / 5 = 200
     */
    e = ma_update(&f, 1000, &avg, NULL);
    CHECK_EQ_INT(e, ERR_OK);
    CHECK_EQ_INT(avg, 200);

    /*
     * After five total samples of 1000, the whole window should be 1000.
     */
    for (int i = 0; i < 4; ++i) {
        e = ma_update(&f, 1000, &avg, NULL);
        CHECK_EQ_INT(e, ERR_OK);
    }

    CHECK_EQ_INT(avg, 1000);
}

static void test_null_args(void)
{
    ma_filter_t f;
    ma_init(&f);

    int32_t avg = 0;

    CHECK_EQ_INT(ma_update(NULL, 1, &avg, NULL), ERR_INVALID_ARG);
    CHECK_EQ_INT(ma_update(&f, 1, NULL, NULL), ERR_INVALID_ARG);
}

int main(void)
{
    RUN(test_warmup);
    RUN(test_running_average);
    RUN(test_null_args);

    return test_report();
}