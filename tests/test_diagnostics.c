#include "core/diagnostics.h"
#include "test_support.h"

static void test_init_and_uptime(void)
{
    diagnostics_t d;
    diag_init(&d, 100u);

    CHECK_EQ_INT(diag_uptime_ms(&d, 100u), 0u);
    CHECK_EQ_INT(diag_uptime_ms(&d, 150u), 50u);
    CHECK_EQ_INT(diag_uptime_ms(&d, 90u), 0u);
}

static void test_counters(void)
{
    diagnostics_t d;
    diag_init(&d, 0u);

    diag_record(&d, 10u, CTRL_STATE_WARMUP, ERR_WARMUP, false, true);
    CHECK_EQ_INT(d.sample_count, 1u);
    CHECK_EQ_INT(d.warmup_count, 1u);
    CHECK_EQ_INT(d.fault_events, 0u);

    diag_record(&d, 20u, CTRL_STATE_RUN, ERR_OK, false, true);
    CHECK_EQ_INT(d.sample_count, 2u);
    CHECK_EQ_INT(d.warmup_count, 1u);
    CHECK_EQ_INT(d.fault_events, 0u);
}

static void test_fault_edge(void)
{
    diagnostics_t d;
    diag_init(&d, 0u);

    diag_record(&d, 10u, CTRL_STATE_RUN, ERR_OK, false, true);
    CHECK_EQ_INT(d.fault_events, 0u);

    diag_record(&d, 20u, CTRL_STATE_RUN, ERR_SENSOR, false, true);
    CHECK_EQ_INT(d.sensor_error_count, 1u);
    CHECK_EQ_INT(d.fault_events, 0u);

    diag_record(&d, 30u, CTRL_STATE_SENSOR_FAULT, ERR_SENSOR, false, true);
    CHECK_EQ_INT(d.fault_events, 1u);

    diag_record(&d, 40u, CTRL_STATE_SENSOR_FAULT, ERR_SENSOR, false, true);
    CHECK_EQ_INT(d.fault_events, 1u);

    diag_record(&d, 50u, CTRL_STATE_RUN, ERR_OK, false, true);
    CHECK_EQ_INT(d.fault_events, 1u);
}

int main(void)
{
    RUN(test_init_and_uptime);
    RUN(test_counters);
    RUN(test_fault_edge);

    return test_report();
}