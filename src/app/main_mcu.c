#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "app/app.h"
#include "app/scheduler.h"
#include "bsp/stm32f1/uart.h"
#include "bsp/i2c.h"
#include "core/diagnostics.h"
#include "driver/stub_sensor.h" // Или sht31, если подключил
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

    ctx->regs->cfg_margin_on_cd = ctx->app->cfg.dew_margin_on_cd;
    ctx->regs->cfg_margin_off_cd = ctx->app->cfg.dew_margin_off_cd;
    ctx->regs->cfg_sample_ms = ctx->app->cfg.sample_period_ms;
}

static void apply_modbus_write_to_app(mcu_ctx_t *ctx, uint16_t reg_addr, uint16_t value)
{
    if (ctx == NULL || ctx->app == NULL) {
        return;
    }

    device_config_t tmp_cfg = ctx->app->cfg;
    bool changed = false;

    switch (reg_addr) {
    case MB_CFG_MARGIN_ON:
        tmp_cfg.dew_margin_on_cd = (int16_t)value;
        changed = true;
        break;
    case MB_CFG_MARGIN_OFF:
        tmp_cfg.dew_margin_off_cd = (int16_t)value;
        changed = true;
        break;
    case MB_CFG_SAMPLE_MS:
        if (value == 0u) {
            return;
        }
        tmp_cfg.sample_period_ms = value;
        changed = true;
        break;
    default:
        return;
    }

    if (changed) {
        storage_finalize(&tmp_cfg);
        err_t e = app_update_config(ctx->app, &tmp_cfg);
        
        if (e != ERR_OK && logger_level_enabled(LOG_LEVEL_WARN)) {
            log_timestamp(platform_millis());
            logger_write_str("CFG SAVE ERROR=");
            logger_write_str(err_str(e));
            logger_new_line();
        }
    }
}

static void handle_modbus_request(mcu_ctx_t *ctx, const modbus_request_t *req)
{
    if (ctx == NULL || req == NULL) {
        return;
    }

    uint8_t resp_buf[260]; // Increased slightly for safety
    size_t resp_len = 0u;

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

        size_t byte_count = (size_t)quantity * 2u;

        if (sizeof(resp_buf) < 5u + byte_count) {
            resp_len = modbus_build_exception(req->slave_addr, 0x03u, MODBUS_EX_SLAVE_DEVICE_FAIL, resp_buf, sizeof(resp_buf));
            break;
        }

        resp_buf[0] = req->slave_addr;
        resp_buf[1] = 0x03u;
        resp_buf[2] = (uint8_t)byte_count;

        if (!mb_regs_read(ctx->regs, start_addr, quantity, &resp_buf[3], byte_count)) {
            resp_len = modbus_build_exception(req->slave_addr, 0x03u, MODBUS_EX_ILLEGAL_DATA_ADDR, resp_buf, sizeof(resp_buf));
            break;
        }

        size_t payload_with_header_len = 3u + byte_count;
        uint16_t crc = modbus_crc16(resp_buf, payload_with_header_len);
        
        resp_buf[payload_with_header_len] = (uint8_t)(crc & 0xFFu);
        resp_buf[payload_with_header_len + 1u] = (uint8_t)(crc >> 8);

        resp_len = payload_with_header_len + 2u;
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

        apply_modbus_write_to_app(ctx, reg_addr, value);

        resp_buf[0] = req->slave_addr;
        resp_buf[1] = 0x06u;
        resp_buf[2] = req->data[0];
        resp_buf[3] = req->data[1];
        resp_buf[4] = req->data[2];
        resp_buf[5] = req->data[3];

        uint16_t crc = modbus_crc16(resp_buf, 6u);
        resp_buf[6] = (uint8_t)(crc & 0xFFu);
        resp_buf[7] = (uint8_t)(crc >> 8);

        resp_len = 8u;
    }
    break;

    default:
        resp_len = modbus_build_exception(req->slave_addr, req->function, MODBUS_EX_ILLEGAL_FUNC, resp_buf, sizeof(resp_buf));
        break;
    }

    if (resp_len > 0u) {
        uart1_tx_bytes(resp_buf, resp_len);
        
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
        
        /* Mark ticket as done */
        app_mark_ticket(c->app, TICKET_CONTROL);
    }
}

static void logger_poll_task(void *ctx)
{
    (void)ctx;
    logger_task();
    platform_poll();
    
    /* Optionally mark LOG ticket if we require it */
    /* app_mark_ticket(..., TICKET_LOG); */ 
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
        
        /* Mark COMM ticket on successful request handling */
        app_mark_ticket(c->app, TICKET_COMM);
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
        
        /* Even on error, we processed the frame, so COMM task ran. 
           Depending on policy, you might still feed WDT here. 
           Let's assume yes, because hang is worse than bad packet. */
        app_mark_ticket(c->app, TICKET_COMM);
    }
}

int main(void)
{
    stm32f1_platform_init();
    
    /* Init Watchdog: 2 second timeout */
    platform_wdg_init(2000u);

    sensor_port_t sensor = {
        .read = stub_sensor_read,
        .ctx = NULL
    };

    app_t app;
    app_init(&app, &sensor);

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

    uint32_t period = app.ctrl.cfg.sample_period_ms;
    
    (void)sched_register(&sched, period, control_task, &ctx, "control");
    (void)sched_register(&sched, 10u, logger_poll_task, NULL, "logger");
    (void)sched_register(&sched, 1u, modbus_task, &ctx, "modbus");

    for (;;) {
        /* Clear tickets BEFORE running tasks for this cycle */
        app_clear_tickets(&app);

        (void)sched_run_once(&sched, platform_millis());

        /* Check if ALL critical tasks completed successfully */
        if (app_can_feed_watchdog(&app)) {
            platform_wdg_feed();
        }
        /* If NOT fed, watchdog will reset MCU soon */
        
        platform_wfi();
    }

    return 0;
}