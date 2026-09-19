#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "app/app.h"
#include "core/errors.h"
#include "driver/mock_sensor.h"
#include "platform/host/platform_host.h"
#include "platform/platform.h"

static const char *state_name(ctrl_state_t s)
{
    switch (s) {
    case CTRL_STATE_INIT:
        return "INIT";
    case CTRL_STATE_WARMUP:
        return "WARMUP";
    case CTRL_STATE_RUN:
        return "RUN";
    case CTRL_STATE_SENSOR_FAULT:
        return "SENSOR_FAULT";
    case CTRL_STATE_MATH_FAULT:
        return "MATH_FAULT";
    case CTRL_STATE_SAFE:
        return "SAFE";
    default:
        return "UNKNOWN";
    }
}

int main(void)
{
    mock_sensor_t mock;
    mock_sensor_init(&mock);

    sensor_port_t sensor;
    sensor.read = mock_sensor_read;
    sensor.ctx = &mock;

    controller_config_t cfg = CONTROLLER_DEFAULTS;

    /*
     * For host demo we sample faster than real device default.
     * Real device may use 1000 ms.
     */
    cfg.sample_period_ms = 50u;

    app_t app;
    app_init(&app, &sensor, &cfg);

    printf("[BOOT] climate-node host runner started\n");

    const uint32_t tick_ms = 10u;
    const uint32_t max_ticks = 300u;

    for (uint32_t i = 0; i < max_ticks; ++i) {
        platform_host_tick(tick_ms);

        uint32_t now = platform_millis();
        bool processed = false;

        err_t e = app_task(&app, now, &processed);

        if (processed) {
            app_report_t r = app_report(&app);

            printf(
                "[%06" PRIu32 "] state=%-13s err=%-13s T=%6ld RH=%5lu DP=%6ld RELAY=%d\n",
                now,
                state_name(r.state),
                err_str(e),
                (long)r.filtered_temp_cd,
                (unsigned long)r.humidity_cp,
                (long)r.dew_point_cd,
                r.relay_on ? 1 : 0);
        }

        platform_wfi();
    }

    app_report_t final = app_report(&app);

    printf(
        "[DONE] samples=%lu faults=%lu\n",
        (unsigned long)final.sample_count,
        (unsigned long)final.fault_count);

    return 0;
}