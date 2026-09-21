#include "app/app.h"
#include <stddef.h>
#include <string.h>
#include "platform/platform.h"

void app_init(app_t *app, const sensor_port_t *sensor)
{
    if (app == NULL || sensor == NULL || sensor->read == NULL) {
        return;
    }

    app->sensor = *sensor;
    
    /* Load persisted config or defaults */
    err_t e = storage_load(&app->cfg);
    if (e != ERR_OK) {
        storage_load_defaults(&app->cfg);
    }

    /* Init controller with loaded config */
    controller_config_t ctrl_cfg = {
        .dew_margin_on_cd = app->cfg.dew_margin_on_cd,
        .dew_margin_off_cd = app->cfg.dew_margin_off_cd,
        .sample_period_ms = app->cfg.sample_period_ms,
        .fault_threshold = 3,
        .recovery_threshold = 3,
        .relay_safe_state = false
    };
    
    ctrl_init(&app->ctrl, &ctrl_cfg);

    app->next_sample_ms = 0u;
    app->relay_prev = app->ctrl.relay_on;

    /* Init tickets */
    app->ticket_mask = 0u;
    /* Require Control and Comm. Log is optional for safety in this example. */
    app->required_tickets = TICKET_CONTROL | TICKET_COMM; 

    platform_relay_set(app->relay_prev);
}

void app_apply_config(app_t *app)
{
    if (app == NULL) {
        return;
    }

    /* Re-init controller with new params without resetting history completely?
     * For safety, full re-init is easier for now.
     */
    controller_config_t ctrl_cfg = {
        .dew_margin_on_cd = app->cfg.dew_margin_on_cd,
        .dew_margin_off_cd = app->cfg.dew_margin_off_cd,
        .sample_period_ms = app->cfg.sample_period_ms,
        .fault_threshold = 3,
        .recovery_threshold = 3,
        .relay_safe_state = false
    };
    
    ctrl_init(&app->ctrl, &ctrl_cfg);
}

err_t app_update_config(app_t *app, const device_config_t *new_cfg)
{
    if (app == NULL || new_cfg == NULL) {
        return ERR_INVALID_ARG;
    }

    /* Validate before saving */
    if (!storage_validate(new_cfg)) {
        return ERR_INVALID_ARG;
    }

    app->cfg = *new_cfg;
    
    err_t save_err = storage_save(&app->cfg);
    if (save_err != ERR_OK) {
        return save_err;
    }

    app_apply_config(app);
    return ERR_OK;
}

err_t app_task(app_t *app, uint32_t now_ms, bool *processed)
{
    if (app == NULL || processed == NULL) {
        return ERR_INVALID_ARG;
    }

    *processed = false;

    if (app->sensor.read == NULL) {
        return ERR_INVALID_ARG;
    }

    if ((int32_t)(now_ms - app->next_sample_ms) < 0) {
        return ERR_OK;
    }

    app->next_sample_ms += app->ctrl.cfg.sample_period_ms;

    if ((int32_t)(app->next_sample_ms - now_ms) < 0) {
        app->next_sample_ms = now_ms + app->ctrl.cfg.sample_period_ms;
    }

    sample_t s = {0};
    err_t e = app->sensor.read(app->sensor.ctx, &s);

    if (e != ERR_OK) {
        s.quality = QUALITY_INVALID;
    }

    s.timestamp_ms = now_ms;

    e = ctrl_update(&app->ctrl, &s);

    bool relay = ctrl_relay_on(&app->ctrl);

    if (relay != app->relay_prev) {
        platform_relay_set(relay);
        app->relay_prev = relay;
    }

    *processed = true;
    return e;
}

app_report_t app_report(const app_t *app)
{
    app_report_t r = {0};

    if (app == NULL) {
        return r;
    }

    r.state = app->ctrl.state;
    r.last_error = app->ctrl.last_error;

    r.filtered_temp_cd = app->ctrl.filtered_temp_cd;
    r.humidity_cp = app->ctrl.humidity_cp;
    r.dew_point_cd = app->ctrl.dew_point_cd;

    r.relay_on = app->ctrl.relay_on;

    r.sample_count = app->ctrl.sample_count;
    r.fault_count = app->ctrl.fault_count;

    return r;
}

bool app_can_feed_watchdog(const app_t *app)
{
    if (app == NULL) return false;
    return (app->ticket_mask & app->required_tickets) == app->required_tickets;
}

void app_clear_tickets(app_t *app)
{
    if (app != NULL) {
        app->ticket_mask = 0u;
    }
}

void app_mark_ticket(app_t *app, uint32_t ticket_id)
{
    if (app != NULL) {
        app->ticket_mask |= ticket_id;
    }
}