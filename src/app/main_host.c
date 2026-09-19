#include <stddef.h>

#include "app/app.h"
#include "app/scheduler.h"
#include "core/diagnostics.h"
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

typedef struct {
    app_t *app;
    diagnostics_t *diag;
} host_ctx_t;

static void log_timestamp(uint32_t ms)
{
    logger_write_char('[');
    logger_write_u32_padded(ms, 6u);
    logger_write_str("] ");
}

static void log_control_result(const app_report_t *r, err_t e)
{
    if (!logger_level_enabled(LOG_LEVEL_INFO)) {
        return;
    }

    log_timestamp(platform_millis());

    logger_write_str("state=");
    logger_write_str(state_name(r->state));

    logger_write_str(" err=");
    logger_write_str(err_str(e));

    logger_write_str(" T=");
    logger_write_fixed_cd(r->filtered_temp_cd);

    logger_write_str(" RH=");
    logger_write_fixed_cp(r->humidity_cp);

    logger_write_str(" DP=");
    logger_write_fixed_cd(r->dew_point_cd);

    logger_write_str(" RELAY=");
    logger_write_char(r->relay_on ? '1' : '0');

    logger_new_line();
}

static void control_task(void *ctx)
{
    host_ctx_t *c = (host_ctx_t *)ctx;

    uint32_t now = platform_millis();
    bool processed = false;

    err_t e = app_task(c->app, now, &processed);

    if (processed) {
        app_report_t r = app_report(c->app);

        diag_record(c->diag, now, r.state, e, r.relay_on, true);
        log_control_result(&r, e);
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

    host_ctx_t *c = (host_ctx_t *)ctx;
    uint32_t now = platform_millis();

    log_timestamp(now);

    logger_write_str("DIAG uptime=");
    logger_write_u32(diag_uptime_ms(c->diag, now));

    logger_write_str(" samples=");
    logger_write_u32(c->diag->sample_count);

    logger_write_str(" fault_events=");
    logger_write_u32(c->diag->fault_events);

    logger_write_str(" sensor_errors=");
    logger_write_u32(c->diag->sensor_error_count);

    logger_write_str(" warmups=");
    logger_write_u32(c->diag->warmup_count);

    logger_write_str(" state=");
    logger_write_str(state_name(c->diag->last_state));

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

    diagnostics_t diag;
    diag_init(&diag, 0u);

    host_ctx_t ctx = {
        .app = &app,
        .diag = &diag
    };

    logger_init();
    logger_set_level(LOG_LEVEL_INFO);

    sched_task_t tasks[4];
    scheduler_t sched;

    sched_init(&sched, tasks, 4u);

    (void)sched_register(&sched, 50u, control_task, &ctx, "control");
    (void)sched_register(&sched, 10u, logger_periodic_task, NULL, "logger");
    (void)sched_register(&sched, 1000u, diagnostics_task, &ctx, "diag");

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

    uint32_t now = platform_millis();

    log_timestamp(now);

    logger_write_str("DONE uptime=");
    logger_write_u32(diag_uptime_ms(&diag, now));

    logger_write_str(" samples=");
    logger_write_u32(diag.sample_count);

    logger_write_str(" fault_events=");
    logger_write_u32(diag.fault_events);

    logger_write_str(" sensor_errors=");
    logger_write_u32(diag.sensor_error_count);

    logger_write_str(" warmups=");
    logger_write_u32(diag.warmup_count);

    logger_new_line();

    logger_flush();

    return 0;
}