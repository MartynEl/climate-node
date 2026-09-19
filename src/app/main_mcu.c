#include <stdbool.h>
#include <stddef.h>

#include "app/app.h"
#include "app/scheduler.h"
#include "core/diagnostics.h"
#include "driver/stub_sensor.h"
#include "platform/platform.h"
#include "platform/stm32f1/platform_stm32f1.h"
#include "service/logger.h"

typedef struct {
    app_t *app;
    diagnostics_t *diag;
} mcu_ctx_t;

static void control_task(void *ctx)
{
    mcu_ctx_t *c = (mcu_ctx_t *)ctx;

    uint32_t now = platform_millis();
    bool processed = false;

    err_t e = app_task(c->app, now, &processed);

    if (processed) {
        app_report_t r = app_report(c->app);
        diag_record(c->diag, now, r.state, e, r.relay_on, true);
    }
}

static void logger_poll_task(void *ctx)
{
    (void)ctx;

    logger_task();
    platform_poll();
}

int main(void)
{
    stm32f1_platform_init();

    sensor_port_t sensor = {
        .read = stub_sensor_read,
        .ctx = NULL
    };

    controller_config_t cfg = CONTROLLER_DEFAULTS;
    cfg.sample_period_ms = 1000u;

    app_t app;
    app_init(&app, &sensor, &cfg);

    diagnostics_t diag;
    diag_init(&diag, platform_millis());

    logger_init();

    mcu_ctx_t ctx = {
        .app = &app,
        .diag = &diag
    };

    sched_task_t tasks[2];
    scheduler_t sched;

    sched_init(&sched, tasks, 2u);

    (void)sched_register(&sched, cfg.sample_period_ms, control_task, &ctx, "control");
    (void)sched_register(&sched, 10u, logger_poll_task, NULL, "logger");

    for (;;) {
        (void)sched_run_once(&sched, platform_millis());
        platform_wfi();
    }

    return 0;
}
