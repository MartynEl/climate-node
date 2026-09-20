#ifndef SERVICE_STORAGE_H
#define SERVICE_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/errors.h"

/*
 * Persistent device configuration.
 * Must be aligned to 4 bytes for efficient flash writing.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;       /* 0xCAFEBABE */
    uint16_t version;     /* Schema version */
    uint16_t size;        /* Size of this struct */
    
    int16_t dew_margin_on_cd;   /* Threshold to turn ON relay (0.01 degC) */
    int16_t dew_margin_off_cd;  /* Threshold to turn OFF relay (0.01 degC) */
    uint16_t sample_period_ms;  /* Sensor polling period */
    
    uint8_t reserved[10];       /* Padding for future expansion / alignment */
    
    uint32_t crc32;             /* CRC of all preceding fields */
} device_config_t;

/*
 * Initialize config from defaults.
 */
void storage_load_defaults(device_config_t *cfg);

/*
 * Validate config structure (magic, size, CRC).
 */
bool storage_validate(const device_config_t *cfg);

void storage_finalize(device_config_t *cfg);

/*
 * Platform-specific load/save functions are declared in bsp or platform layer.
 * Here we define the logical interface used by the app.
 */
err_t storage_save(const device_config_t *cfg);
err_t storage_load(device_config_t *cfg);

#endif // SERVICE_STORAGE_H