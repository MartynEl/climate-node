#include "core/filter.h"

#include <stddef.h>

void ma_init(ma_filter_t *f)
{
    if (f == NULL) {
        return;
    }

    for (uint8_t i = 0; i < MA_FILTER_SIZE; ++i) {
        f->buf[i] = 0;
    }

    f->sum = 0;
    f->head = 0;
    f->count = 0;
}

err_t ma_update(ma_filter_t *f, int32_t x, int32_t *out_avg, uint8_t *out_count)
{
    if (f == NULL || out_avg == NULL) {
        return ERR_INVALID_ARG;
    }

    if (f->count < MA_FILTER_SIZE) {
        f->buf[f->head] = x;
        f->sum += x;
        f->count++;
    } else {
        f->sum -= f->buf[f->head];
        f->buf[f->head] = x;
        f->sum += x;
    }

    f->head++;
    if (f->head >= MA_FILTER_SIZE) {
        f->head = 0;
    }

    *out_avg = f->sum / (int32_t)f->count;

    if (out_count != NULL) {
        *out_count = f->count;
    }

    if (f->count >= MA_WARMUP_SAMPLES) {
        return ERR_OK;
    }

    return ERR_WARMUP;
}