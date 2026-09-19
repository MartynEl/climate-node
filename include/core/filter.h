#ifndef CORE_FILTER_H
#define CORE_FILTER_H

#include "core/errors.h"

#include <stdint.h>

enum {
    MA_FILTER_SIZE = 5
};

enum {
    MA_WARMUP_SAMPLES = MA_FILTER_SIZE
};

typedef struct {
    int32_t buf[MA_FILTER_SIZE];
    int32_t sum;
    uint8_t head;
    uint8_t count;
} ma_filter_t;

void ma_init(ma_filter_t *f);

/*
 * Returns:
 *   ERR_OK      - filter is warmed up, out_avg is valid for control
 *   ERR_WARMUP  - out_avg is valid as partial average, but not ready for control
 *   ERR_INVALID_ARG - null pointer
 */
err_t ma_update(ma_filter_t *f, int32_t x, int32_t *out_avg, uint8_t *out_count);

#endif // CORE_FILTER_H