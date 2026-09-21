#ifndef DRIVER_SENSOR_SHT31_H
#define DRIVER_SENSOR_SHT31_H

#include "driver/sensor.h"

/**
 * Sensor port implementation for Sensirion SHT31 via I2C.
 * Uses fixed-point conversion as defined in core/types.h.
 */
err_t sht31_read_sample(void *ctx, sample_t *out);

#endif // DRIVER_SENSOR_SHT31_H
