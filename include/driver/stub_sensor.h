#ifndef DRIVER_STUB_SENSOR_H
#define DRIVER_STUB_SENSOR_H

#include "driver/sensor.h"

err_t stub_sensor_read(void *ctx, sample_t *out);

#endif // DRIVER_STUB_SENSOR_H
