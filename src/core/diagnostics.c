#include "core/diagnostics.h"

#include <stddef.h>

bool diag_is_fault_state(ctrl_state_t state)
{
    return state == CTRL_STATE_SENSOR_FAULT ||
           state == CTRL_STATE_MATH_FAULT ||
           state == CTRL_STATE_SAFE;
}

void diag_init(diagnostics_t *d, uint32_t now_ms)
{
    if (d == NULL) {
        return;
    }

    d->started_ms = now_ms;
    d->last_sample_ms = now_ms;

    d->sample_count = 0u;
    d->fault_events = 0u;
    d->sensor_error_count = 0u;
    d->warmup_count = 0u;

    d->last_error = ERR_NOT_READY;
    d->last_state = CTRL_STATE_INIT;
    d->relay_on = false;
    d->initialized = false;
}

void diag_record(
    diagnostics_t *d,
    uint32_t now_ms,
    ctrl_state_t state,
    err_t err,
    bool relay_on,
    bool processed)
{
    if (d == NULL) {
        return;
    }

    /*
     * Important: capture previous state before updating current fields.
     * fault_events counts edges:
     *   non-fault state -> fault state
     */
    ctrl_state_t prev_state = d->last_state;
    bool prev_initialized = d->initialized;

    d->last_error = err;
    d->relay_on = relay_on;

    if (!processed) {
        return;
    }

    d->sample_count++;
    d->last_sample_ms = now_ms;

    if (err == ERR_WARMUP) {
        d->warmup_count++;
    }

    if (err == ERR_SENSOR) {
        d->sensor_error_count++;
    }

    bool is_fault = diag_is_fault_state(state);
    bool was_fault = prev_initialized && diag_is_fault_state(prev_state);

    if (is_fault && !was_fault) {
        d->fault_events++;
    }

    d->last_state = state;
    d->initialized = true;
}

uint32_t diag_uptime_ms(const diagnostics_t *d, uint32_t now_ms)
{
    if (d == NULL) {
        return 0u;
    }

    if (now_ms < d->started_ms) {
        return 0u;
    }

    return now_ms - d->started_ms;
}
