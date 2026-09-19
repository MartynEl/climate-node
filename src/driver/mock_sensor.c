#include "driver/mock_sensor.h"

#include <stddef.h>

void mock_sensor_init(mock_sensor_t *s)
{
    if (s != NULL) {
        s->step = 0;
    }
}

err_t mock_sensor_read(void *ctx, sample_t *out)
{
    mock_sensor_t *s = (mock_sensor_t *)ctx;

    if (s == NULL || out == NULL) {
        return ERR_INVALID_ARG;
    }

    /*
     * Simple deterministic pseudo-noise.
     * No math.h, no float, suitable for embedded-style logic.
     */
    uint32_t x = s->step * 1103515245u + 12345u;
    x ^= x >> 16;

    uint32_t noise_u = x % 101u;          /* 0..100 */
    int32_t noise = (int32_t)noise_u - 50; /* -50..50, i.e. -0.50..+0.50 degC */

    out->temp_cd = 2250 + noise; /* around 22.50 degC */
    out->rh_cp = 6500;           /* 65.00 % */
    out->timestamp_ms = 0;

    /*
     * Inject sensor fault for steps 20..24.
     * Controller has fault debounce threshold = 3,
     * so this should trigger SENSOR_FAULT and later recovery.
     */
    if (s->step >= 20u && s->step < 25u) {
        out->quality = QUALITY_INVALID;
    } else {
        out->quality = QUALITY_VALID;
    }

    s->step++;

    return ERR_OK;
}