#include "service/storage.h"

#include <stddef.h>
#include <string.h>

/* Simple software CRC32 implementation (standard polynomial) */
static uint32_t calc_crc32(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0u; i < len; ++i) {
        crc ^= bytes[i];
        for (uint8_t j = 0u; j < 8u; ++j) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

void storage_load_defaults(device_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));

    cfg->magic = 0xCAFEBABEU;
    cfg->version = 1u;
    cfg->size = sizeof(device_config_t);
    
    /* Defaults matching CONTROLLER_DEFAULTS */
    cfg->dew_margin_on_cd = 500;   /* 5.00 C */
    cfg->dew_margin_off_cd = 700;  /* 7.00 C */
    cfg->sample_period_ms = 1000u;
    
    /* Calculate CRC over everything except the crc field itself */
    size_t data_len = offsetof(device_config_t, crc32);
    cfg->crc32 = calc_crc32(cfg, data_len);
}

bool storage_validate(const device_config_t *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    if (cfg->magic != 0xCAFEBABEU) {
        return false;
    }

    if (cfg->size != sizeof(device_config_t)) {
        return false;
    }

    size_t data_len = offsetof(device_config_t, crc32);
    uint32_t expected_crc = calc_crc32(cfg, data_len);

    return cfg->crc32 == expected_crc;
}

/* 
 * These are weak symbols or stubs here. 
 * The real implementation depends on the platform (Host vs STM32 Flash).
 * For now, we declare them extern and implement in platform layers.
 */
extern err_t platform_flash_read_config(uint32_t slot_index, device_config_t *out);
extern err_t platform_flash_write_config(uint32_t slot_index, const device_config_t *cfg);

err_t storage_load(device_config_t *cfg)
{
    if (cfg == NULL) {
        return ERR_INVALID_ARG;
    }

    /* Try Slot 0 first, then Slot 1 */
    err_t e0 = platform_flash_read_config(0, cfg);
    if (e0 == ERR_OK && storage_validate(cfg)) {
        return ERR_OK;
    }

    err_t e1 = platform_flash_read_config(1, cfg);
    if (e1 == ERR_OK && storage_validate(cfg)) {
        return ERR_OK;
    }

    /* Both failed or invalid -> Load defaults */
    storage_load_defaults(cfg);
    return ERR_NOT_READY; /* Indicate that defaults were loaded due to error */
}

err_t storage_save(const device_config_t *cfg)
{
    if (cfg == NULL || !storage_validate(cfg)) {
        return ERR_INVALID_ARG;
    }

    /* 
     * Strategy: Write to alternating slots.
     * To know which slot is currently active, we'd need a counter in metadata.
     * For simplicity in this skeleton: always write to Slot 0, keep Slot 1 as backup.
     * In production: rotate slots based on sequence number.
     */
    return platform_flash_write_config(0, cfg);
}

void storage_finalize(device_config_t *cfg)
{
    if (cfg == NULL) return;
    cfg->magic = 0xCAFEBABEU;
    cfg->version = 1u;
    cfg->size = sizeof(device_config_t);
    size_t data_len = offsetof(device_config_t, crc32);
    cfg->crc32 = calc_crc32(cfg, data_len);
}