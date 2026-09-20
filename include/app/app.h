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
} app_t;

void app_init(app_t *app, const sensor_port_t *sensor);

/* Apply current config to controller */
void app_apply_config(app_t *app);

err_t app_task(app_t *app, uint32_t now_ms, bool *processed);

app_report_t app_report(const app_t *app);

/* Update config and save */
err_t app_update_config(app_t *app, const device_config_t *new_cfg);

#endif // APP_APP_H
