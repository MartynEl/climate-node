#ifndef DRIVER_SENSOR_H
#define DRIVER_SENSOR_H

#include "core/errors.h"
#include "core/types.h"

typedef err_t (*sensor_read_fn)(void *ctx, sample_t *out);

typedef struct {
    sensor_read_fn read;
    void *ctx;
} sensor_port_t;

#endif // DRIVER_SENSOR_H