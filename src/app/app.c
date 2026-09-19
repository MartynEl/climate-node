#include "app/app.h"

#include <stddef.h>

#include "platform/platform.h"

void app_init(app_t *app, const sensor_port_t *sensor, const controller_config_t *cfg)
{
    if (app == NULL || sensor == NULL || sensor->read == NULL) {
        return;
    }

    ctrl_init(&app->ctrl, cfg);

    app->sensor = *sensor;
    app->sample_period_ms = app->ctrl.cfg.sample_period_ms;

    if (app->sample_period_ms == 0u) {
        app->sample_period_ms = 1000u;
    }

    app->next_sample_ms = 0u;
    app->relay_prev = app->ctrl.relay_on;

    platform_relay_set(app->relay_prev);
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

    /*
     * Non-blocking periodic task.
     * Use signed comparison to handle uint32_t wrap correctly.
     */
    if ((int32_t)(now_ms - app->next_sample_ms) < 0) {
        return ERR_OK;
    }

    app->next_sample_ms += app->sample_period_ms;

    /*
     * If we missed several periods, do not try to catch up by spinning.
     * Reschedule from now.
     */
    if ((int32_t)(app->next_sample_ms - now_ms) < 0) {
        app->next_sample_ms = now_ms + app->sample_period_ms;
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

    platform_watchdog_feed();

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