#include "core/controller.h"

#include <stddef.h>

#include "core/dewpoint.h"

const controller_config_t CONTROLLER_DEFAULTS = {
    .dew_margin_on_cd = 500,            /* 5.00 degC */
    .dew_margin_off_cd = 700,           /* 7.00 degC */
    .sample_period_ms = 1000,
    .fault_threshold = 3,
    .recovery_threshold = 3,
    .relay_safe_state = false
};

static void ctrl_set_relay(controller_t *c, bool on)
{
    c->relay_on = on;
}

static bool sample_is_valid(const sample_t *s)
{
    if (s == NULL) {
        return false;
    }

    if (s->quality != QUALITY_VALID) {
        return false;
    }

    if (s->temp_cd < TEMP_CD_MIN || s->temp_cd > TEMP_CD_MAX) {
        return false;
    }

    if (s->rh_cp > RH_CP_MAX) {
        return false;
    }

    return true;
}

void ctrl_init(controller_t *c, const controller_config_t *cfg)
{
    if (c == NULL) {
        return;
    }

    c->cfg = (cfg != NULL) ? *cfg : CONTROLLER_DEFAULTS;

    if (c->cfg.dew_margin_on_cd >= c->cfg.dew_margin_off_cd) {
        c->cfg.dew_margin_on_cd = CONTROLLER_DEFAULTS.dew_margin_on_cd;
        c->cfg.dew_margin_off_cd = CONTROLLER_DEFAULTS.dew_margin_off_cd;
    }

    if (c->cfg.fault_threshold == 0U) {
        c->cfg.fault_threshold = 1U;
    }

    if (c->cfg.recovery_threshold == 0U) {
        c->cfg.recovery_threshold = 1U;
    }

    if (c->cfg.sample_period_ms == 0U) {
        c->cfg.sample_period_ms = 1000U;
    }

    ma_init(&c->filter);

    c->filtered_temp_cd = 0;
    c->humidity_cp = 0;
    c->dew_point_cd = 0;

    c->sensor_error_streak = 0;
    c->sensor_ok_streak = 0;

    c->relay_on = c->cfg.relay_safe_state;

    c->state = CTRL_STATE_INIT;
    c->last_error = ERR_NOT_READY;

    c->sample_count = 0;
    c->fault_count = 0;
}

err_t ctrl_update(controller_t *c, const sample_t *s)
{
    if (c == NULL || s == NULL) {
        return ERR_INVALID_ARG;
    }

    c->sample_count++;

    if (!sample_is_valid(s)) {
        c->sensor_error_streak++;
        c->sensor_ok_streak = 0;
        c->last_error = ERR_SENSOR;

        if (c->sensor_error_streak >= c->cfg.fault_threshold) {
            c->state = CTRL_STATE_SENSOR_FAULT;
            c->fault_count++;
            ctrl_set_relay(c, c->cfg.relay_safe_state);
        }

        return ERR_SENSOR;
    }

    c->sensor_error_streak = 0;

    if (c->state == CTRL_STATE_SENSOR_FAULT || c->state == CTRL_STATE_MATH_FAULT) {
        c->sensor_ok_streak++;

        if (c->sensor_ok_streak < c->cfg.recovery_threshold) {
            c->last_error = ERR_NOT_READY;
            return ERR_NOT_READY;
        }
    }

    c->sensor_ok_streak = 0;
    c->humidity_cp = s->rh_cp;

    err_t ferr = ma_update(&c->filter, s->temp_cd, &c->filtered_temp_cd, NULL);

    if (ferr == ERR_WARMUP) {
        c->state = CTRL_STATE_WARMUP;
        c->last_error = ERR_WARMUP;
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return ERR_WARMUP;
    }

    if (ferr != ERR_OK) {
        c->state = CTRL_STATE_MATH_FAULT;
        c->last_error = ferr;
        c->fault_count++;
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return ferr;
    }

    err_t derr = dewpoint_calc_cd(c->filtered_temp_cd, c->humidity_cp, &c->dew_point_cd);

    if (derr != ERR_OK) {
        c->state = CTRL_STATE_MATH_FAULT;
        c->last_error = derr;
        c->fault_count++;
        ctrl_set_relay(c, c->cfg.relay_safe_state);
        return derr;
    }

    temp_cd_t margin_cd = c->filtered_temp_cd - c->dew_point_cd;

    if (!c->relay_on) {
        if (margin_cd < c->cfg.dew_margin_on_cd) {
            ctrl_set_relay(c, true);
        }
    } else {
        if (margin_cd > c->cfg.dew_margin_off_cd) {
            ctrl_set_relay(c, false);
        }
    }

    c->state = CTRL_STATE_RUN;
    c->last_error = ERR_OK;

    return ERR_OK;
}

bool ctrl_relay_on(const controller_t *c)
{
    if (c == NULL) {
        return false;
    }

    return c->relay_on;
}