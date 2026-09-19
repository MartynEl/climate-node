#include <stddef.h>

#include "app/app.h"
#include "app/scheduler.h"
#include "core/errors.h"
#include "driver/mock_sensor.h"
#include "platform/host/platform_host.h"
#include "platform/platform.h"
#include "service/logger.h"

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

static void log_timestamp(uint32_t ms)
{
    logger_write_char('[');
    logger_write_u32_padded(ms, 6u);
    logger_write_str("] ");
}

static void log_control_result(const app_t *app, err_t e)
{
    if (!logger_level_enabled(LOG_LEVEL_INFO)) {
        return;
    }

    app_report_t r = app_report(app);

    log_timestamp(platform_millis());

    logger_write_str("state=");
    logger_write_str(state_name(r.state));

    logger_write_str(" err=");
    logger_write_str(err_str(e));

    logger_write_str(" T=");
    logger_write_fixed_cd(r.filtered_temp_cd);

    logger_write_str(" RH=");
    logger_write_fixed_cp(r.humidity_cp);

    logger_write_str(" DP=");
    logger_write_fixed_cd(r.dew_point_cd);

    logger_write_str(" RELAY=");
    logger_write_char(r.relay_on ? '1' : '0');

    logger_new_line();
}

static void control_task(void *ctx)
{
    app_t *app = (app_t *)ctx;

    uint32_t now = platform_millis();
    bool processed = false;

    err_t e = app_task(app, now, &processed);

    if (processed) {
        log_control_result(app, e);
    }
}

static void logger_periodic_task(void *ctx)
{
    (void)ctx;
    logger_task();
}

static void diagnostics_task(void *ctx)
{
    if (!logger_level_enabled(LOG_LEVEL_INFO)) {
        return;
    }

    app_t *app = (app_t *)ctx;
    app_report_t r = app_report(app);

    log_timestamp(platform_millis());

    logger_write_str("DIAG samples=");
    logger_write_u32(r.sample_count);

    logger_write_str(" faults=");
    logger_write_u32(r.fault_count);

    logger_write_str(" state=");
    logger_write_str(state_name(r.state));

    logger_new_line();

    logger_task();
}

int main(void)
{
    mock_sensor_t mock;
    mock_sensor_init(&mock);

    sensor_port_t sensor = {
        .read = mock_sensor_read,
        .ctx = &mock
    };

    controller_config_t cfg = CONTROLLER_DEFAULTS;
    cfg.sample_period_ms = 50u;

    app_t app;
    app_init(&app, &sensor, &cfg);

    logger_init();
    logger_set_level(LOG_LEVEL_INFO);

    sched_task_t tasks[4];
    scheduler_t sched;

    sched_init(&sched, tasks, 4u);

    (void)sched_register(&sched, 50u, control_task, &app, "control");
    (void)sched_register(&sched, 10u, logger_periodic_task, NULL, "logger");
    (void)sched_register(&sched, 1000u, diagnostics_task, &app, "diag");

    logger_write_str("[BOOT] climate-node host runner started");
    logger_new_line();
    logger_flush();

    const uint32_t tick_ms = 10u;
    const uint32_t max_ticks = 300u;

    for (uint32_t i = 0u; i < max_ticks; ++i) {
        platform_host_tick(tick_ms);

        (void)sched_run_once(&sched, platform_millis());

        platform_wfi();
    }

    logger_flush();

    app_report_t final = app_report(&app);

    log_timestamp(platform_millis());

    logger_write_str("DONE samples=");
    logger_write_u32(final.sample_count);

    logger_write_str(" faults=");
    logger_write_u32(final.fault_count);

    logger_new_line();

    logger_flush();

    return 0;
}