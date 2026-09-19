#include <stdbool.h>
#include <stddef.h>

#include "app/app.h"
#include "app/scheduler.h"
#include "bsp/stm32f1/uart.h"
#include "core/diagnostics.h"
#include "driver/stub_sensor.h"
#include "platform/platform.h"
#include "platform/stm32f1/platform_stm32f1.h"
#include "service/logger.h"
#include "service/modbus_server.h"

typedef struct {
    app_t *app;
    diagnostics_t *diag;
} mcu_ctx_t;

static const char hex_digits[] = "0123456789ABCDEF";

static void log_hex_byte(uint8_t b)
{
    logger_write_char(hex_digits[(b >> 4) & 0x0Fu]);
    logger_write_char(hex_digits[b & 0x0Fu]);
}

static void log_timestamp(uint32_t ms)
{
    logger_write_char('[');
    logger_write_u32_padded(ms, 6u);
    logger_write_str("] ");
}

static void log_modbus_event(const modbus_server_t *s)
{
    if (!logger_level_enabled(LOG_LEVEL_INFO)) {
        return;
    }

    log_timestamp(platform_millis());

    logger_write_str("MODBUS event=");
    logger_write_str(modbus_event_str(s->event));

    if (s->event == MODBUS_EVENT_REQUEST) {
        logger_write_str(" addr=");
        logger_write_u32(s->request.slave_addr);

        logger_write_str(" func=");
        logger_write_u32(s->request.function);

        logger_write_str(" len=");
        logger_write_u32((uint32_t)s->request.data_len);

        logger_write_str(" data=");

        size_t n = s->request.data_len;
        if (n > 8u) {
            n = 8u;
        }

        for (size_t i = 0u; i < n; ++i) {
            log_hex_byte(s->request.data[i]);
        }

        if (s->request.data_len > 8u) {
            logger_write_str("...");
        }
    } else {
        logger_write_str(" parse=");
        logger_write_str(modbus_parse_err_str(s->parse_error));

        logger_write_str(" frame_len=");
        logger_write_u32((uint32_t)s->len);
    }

    logger_new_line();
}

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

static void modbus_task(void *ctx)
{
    modbus_server_t *s = (modbus_server_t *)ctx;

    uint32_t now = platform_millis();

    (void)modbus_server_task(s, now);

    if (s->event != MODBUS_EVENT_NONE) {
        log_modbus_event(s);
        modbus_server_clear_event(s);
    }
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

    modbus_server_t modbus;
    modbus_server_init(&modbus, 1u, 5u, uart1_rx_pop, NULL);

    mcu_ctx_t ctx = {
        .app = &app,
        .diag = &diag
    };

    sched_task_t tasks[3];
    scheduler_t sched;

    sched_init(&sched, tasks, 3u);

    (void)sched_register(&sched, cfg.sample_period_ms, control_task, &ctx, "control");
    (void)sched_register(&sched, 10u, logger_poll_task, NULL, "logger");
    (void)sched_register(&sched, 1u, modbus_task, &modbus, "modbus");

    for (;;) {
        (void)sched_run_once(&sched, platform_millis());
        platform_wfi();
    }

    return 0;
}
