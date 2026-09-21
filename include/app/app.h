#ifndef APP_APP_H
#define APP_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "core/controller.h"
#include "driver/sensor.h"
#include "service/storage.h"

typedef struct {
    ctrl_state_t state;
    err_t last_error;

    temp_cd_t filtered_temp_cd;
    rh_cp_t humidity_cp;
    temp_cd_t dew_point_cd;

    bool relay_on;

    uint32_t sample_count;
    uint32_t fault_count;
} app_report_t;

typedef struct {
    controller_t ctrl;
    sensor_port_t sensor;
    device_config_t cfg;
    uint32_t next_sample_ms;
    bool relay_prev;
    
    /* NEW: Watchdog Tickets */
    uint32_t ticket_mask;      /* Bitmask of completed tasks */
    uint32_t required_tickets; /* Mask of tasks that MUST complete */
} app_t;

/* Task IDs for tickets */
enum {
    TICKET_CONTROL = (1u << 0),
    TICKET_COMM    = (1u << 1), /* Modbus processing */
    TICKET_LOG     = (1u << 2)  /* Logger flush */
};

void app_init(app_t *app, const sensor_port_t *sensor);

/* Apply current config to controller */
void app_apply_config(app_t *app);

bool app_can_feed_watchdog(const app_t *app);
void app_clear_tickets(app_t *app);
void app_mark_ticket(app_t *app, uint32_t ticket_id);

err_t app_task(app_t *app, uint32_t now_ms, bool *processed);

app_report_t app_report(const app_t *app);

/* Update config and save */
err_t app_update_config(app_t *app, const device_config_t *new_cfg);

#endif // APP_APP_H
