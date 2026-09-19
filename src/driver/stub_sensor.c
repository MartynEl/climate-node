#include "driver/stub_sensor.h"

#include <stddef.h>

#include "platform/platform.h"

static uint32_t g_stub_step = 0u;

err_t stub_sensor_read(void *ctx, sample_t *out)
{
    (void)ctx;

    if (out == NULL) {
        return ERR_INVALID_ARG;
    }

    uint32_t step = g_stub_step++;
    int32_t noise = (int32_t)((step * 7u) % 21u) - 10;

    out->temp_cd = (temp_cd_t)(2250 + noise);
    out->rh_cp = 6500;
    out->timestamp_ms = platform_millis();
    out->quality = QUALITY_VALID;

    return ERR_OK;
}
