#ifndef CORE_CONTROLLER_H
#define CORE_CONTROLLER_H

#include "core/errors.h"
#include "core/filter.h"
#include "core/types.h"

typedef enum {
    CTRL_STATE_INIT = 0,
    CTRL_STATE_WARMUP,
    CTRL_STATE_RUN,
    CTRL_STATE_SENSOR_FAULT,
    CTRL_STATE_MATH_FAULT,
    CTRL_STATE_SAFE
} ctrl_state_t;

typedef struct {
    temp_cd_t dew_margin_on_cd;
    temp_cd_t dew_margin_off_cd;
    time_ms_t sample_period_ms;
    uint8_t fault_threshold;
    uint8_t recovery_threshold;
    bool relay_safe_state;
} controller_config_t;

extern const controller_config_t CONTROLLER_DEFAULTS;

typedef struct {
    controller_config_t cfg;

    ma_filter_t filter;

    temp_cd_t filtered_temp_cd;
    rh_cp_t humidity_cp;
    temp_cd_t dew_point_cd;

    uint8_t sensor_error_streak;
    uint8_t sensor_ok_streak;

    bool relay_on;

    ctrl_state_t state;
    err_t last_error;

    uint32_t sample_count;
    uint32_t fault_count;
} controller_t;

void ctrl_init(controller_t *c, const controller_config_t *cfg);

/*
 * Processes one sensor sample.
 *
 * Returns:
 *   ERR_OK        - control decision is valid
 *   ERR_WARMUP    - filter is warming up, output is in safe state
 *   ERR_SENSOR    - invalid sensor sample or sensor fault
 *   ERR_NOT_READY - recovering from fault, not yet ready
 *   ERR_MATH      - numeric calculation fault
 *   ERR_INVALID_ARG - null pointer
 */
err_t ctrl_update(controller_t *c, const sample_t *s);

bool ctrl_relay_on(const controller_t *c);

static inline ctrl_state_t ctrl_get_state(const controller_t *c)
{
    return c->state;
}

#endif // CORE_CONTROLLER_H