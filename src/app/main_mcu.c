#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "app/app.h"
#include "app/scheduler.h"
#include "bsp/stm32f1/uart.h"
#include "core/diagnostics.h"
#include "driver/stub_sensor.h"
#include "platform/platform.h"
#include "platform/stm32f1/platform_stm32f1.h"
#include "service/logger.h"
#include "service/modbus.h"
#include "service/modbus_regs.h"
#include "service/modbus_server.h"

typedef struct {
    app_t *app;
    diagnostics_t *diag;
    modbus_reg_map_t *regs;
} mcu_ctx_t;

static void log_timestamp(uint32_t ms)
{
    logger_write_char('[');
    logger_write_u32_padded(ms, 6u);
    logger_write_str("] ");
}

static void sync_registers(mcu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->app == NULL || ctx->diag == NULL || ctx->regs == NULL) {
        return;
    }

    app_report_t r = app_report(ctx->app);

    ctx->regs->temp_filt_cd = (int16_t)r.filtered_temp_cd;
    ctx->regs->rh_cp = (uint16_t)r.humidity_cp;
    ctx->regs->dew_point_cd = (int16_t)r.dew_point_cd;
    ctx->regs->relay_on = r.relay_on;
    ctx->regs->dev_status = (uint16_t)r.state;
    ctx->regs->err_count = (uint16_t)ctx->diag->fault_events;
    ctx->regs->uptime_ms = diag_uptime_ms(ctx->diag, platform_millis());
}

static void handle_modbus_request(mcu_ctx_t *ctx, const modbus_request_t *req)
{
    if (ctx == NULL || req == NULL) {
        return;
    }

    uint8_t resp_buf[256];
    size_t resp_len = 0u;

    /* Sync latest state into registers before reading/writing */
    sync_registers(ctx);

    switch (req->function) {
    case 0x03u: /* Read Holding Registers */
    {
        if (req->data_len != 4u) {
            resp_len = modbus_build_exception(req->slave_addr, 0x03u, MODBUS_EX_ILLEGAL_DATA_VAL, resp_buf, sizeof(resp_buf));
            break;
        }

        uint16_t start_addr = ((uint16_t)req->data[0] << 8) | req->data[1];
        uint16_t quantity = ((uint16_t)req->data[2] << 8) | req->data[3];

        if (quantity == 0u || quantity > 125u) {
            resp_len = modbus_build_exception(req->slave_addr, 0x03u, MODBUS_EX_ILLEGAL_DATA_VAL, resp_buf, sizeof(resp_buf));
            break;
        }

        /* Calculate byte count for payload */
        size_t byte_count = (size_t)quantity * 2u;
        
        /* We need space for Byte Count field + Data */
        if (resp_len < 1u + byte_count + 2u) { /* Simplified check */
             /* Actually we build manually below */
        }

        uint8_t data_payload[250]; /* Max 125 regs * 2 bytes = 250 */
        
        if (!mb_regs_read(ctx->regs, start_addr, quantity, data_payload, sizeof(data_payload))) {
            resp_len = modbus_build_exception(req->slave_addr, 0x03u, MODBUS_EX_ILLEGAL_DATA_ADDR, resp_buf, sizeof(resp_buf));
            break;
        }

        /* Build response: [ByteCount] [Data...] */
        uint8_t final_payload[251];
        final_payload[0] = (uint8_t)byte_count;
        memcpy(&final_payload[1], data_payload, byte_count);

        resp_len = modbus_build_response(req->slave_addr, 0x03u, final_payload, 1u + byte_count, resp_buf, sizeof(resp_buf));
    }
    break;

    case 0x06u: /* Write Single Register */
    {
        if (req->data_len != 4u) {
            resp_len = modbus_build_exception(req->slave_addr, 0x06u, MODBUS_EX_ILLEGAL_DATA_VAL, resp_buf, sizeof(resp_buf));
            break;
        }

        uint16_t reg_addr = ((uint16_t)req->data[0] << 8) | req->data[1];
        uint16_t value = ((uint16_t)req->data[2] << 8) | req->data[3];

        if (!mb_regs_write_single(ctx->regs, reg_addr, value)) {
            resp_len = modbus_build_exception(req->slave_addr, 0x06u, MODBUS_EX_ILLEGAL_DATA_ADDR, resp_buf, sizeof(resp_buf));
            break;
        }

        /* Echo back the request as confirmation */
        resp_len = modbus_build_response(req->slave_addr, 0x06u, req->data, 4u, resp_buf, sizeof(resp_buf));
    }
    break;

    default:
        resp_len = modbus_build_exception(req->slave_addr, req->function, MODBUS_EX_ILLEGAL_FUNC, resp_buf, sizeof(resp_buf));
        break;
    }

    if (resp_len > 0u) {
        uart1_tx_bytes(resp_buf, resp_len);
        
        /* Optional: Log sent response length */
        if (logger_level_enabled(LOG_LEVEL_DEBUG)) {
            log_timestamp(platform_millis());
            logger_write_str("TX len=");
            logger_write_u32((uint32_t)resp_len);
            logger_new_line();
        }
    }
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
    mcu_ctx_t *c = (mcu_ctx_t *)ctx;
    static modbus_server_t srv;
    static bool initialized = false;

    if (!initialized) {
        modbus_server_init(&srv, 1u, 5u, uart1_rx_pop, NULL);
        initialized = true;
    }

    uint32_t now = platform_millis();
    (void)modbus_server_task(&srv, now);

    if (srv.event == MODBUS_EVENT_REQUEST) {
        handle_modbus_request(c, &srv.request);
        modbus_server_clear_event(&srv);
    } else if (srv.event != MODBUS_EVENT_NONE) {
        /* Log errors */
        if (logger_level_enabled(LOG_LEVEL_WARN)) {
            log_timestamp(now);
            logger_write_str("RX ERR event=");
            logger_write_str(modbus_event_str(srv.event));
            logger_write_str(" parse=");
            logger_write_str(modbus_parse_err_str(srv.parse_error));
            logger_new_line();
        }
        modbus_server_clear_event(&srv);
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

    modbus_reg_map_t regs;
    mb_regs_init(&regs);

    mcu_ctx_t ctx = {
        .app = &app,
        .diag = &diag,
        .regs = &regs
    };

    sched_task_t tasks[3];
    scheduler_t sched;

    sched_init(&sched, tasks, 3u);

    (void)sched_register(&sched, cfg.sample_period_ms, control_task, &ctx, "control");
    (void)sched_register(&sched, 10u, logger_poll_task, NULL, "logger");
    (void)sched_register(&sched, 1u, modbus_task, &ctx, "modbus");

    for (;;) {
        (void)sched_run_once(&sched, platform_millis());
        platform_wfi();
    }

    return 0;
}
