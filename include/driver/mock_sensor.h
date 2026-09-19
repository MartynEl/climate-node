#ifndef DRIVER_MOCK_SENSOR_H
#define DRIVER_MOCK_SENSOR_H

#include "driver/sensor.h"

typedef struct {
    uint32_t step;
} mock_sensor_t;

void mock_sensor_init(mock_sensor_t *s);
err_t mock_sensor_read(void *ctx, sample_t *out);

#endif // DRIVER_MOCK_SENSOR_H