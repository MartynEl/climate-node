#ifndef CORE_DIAGNOSTICS_H
#define CORE_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/controller.h"
#include "core/errors.h"

typedef struct {
    uint32_t started_ms;
    uint32_t last_sample_ms;

    uint32_t sample_count;
    uint32_t fault_events;
    uint32_t sensor_error_count;
    uint32_t warmup_count;

    err_t last_error;
    ctrl_state_t last_state;
    bool relay_on;
    bool initialized;
} diagnostics_t;

void diag_init(diagnostics_t *d, uint32_t now_ms);

void diag_record(
    diagnostics_t *d,
    uint32_t now_ms,
    ctrl_state_t state,
    err_t err,
    bool relay_on,
    bool processed);

uint32_t diag_uptime_ms(const diagnostics_t *d, uint32_t now_ms);

bool diag_is_fault_state(ctrl_state_t state);

#endif // CORE_DIAGNOSTICS_H