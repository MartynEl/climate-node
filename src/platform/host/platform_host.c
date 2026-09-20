#include "platform/platform.h"
#include "platform/host/platform_host.h"
#include "service/storage.h"

#include <stdio.h>
#include <string.h>

static uint32_t g_now_ms = 0;
static bool g_relay = false;

/* Emulated Flash: Two slots of sizeof(device_config_t) each */
#define MOCK_FLASH_SIZE (sizeof(device_config_t) * 2)
static uint8_t g_mock_flash[MOCK_FLASH_SIZE];
static uint32_t g_mock_write_count = 0;

uint32_t platform_millis(void)
{
    return g_now_ms;
}

void platform_host_tick(uint32_t ms)
{
    g_now_ms += ms;
}

void platform_wfi(void)
{
    /* Host: nothing to do. */
}

void platform_watchdog_feed(void)
{
    /* Host: watchdog is not implemented yet. */
}

void platform_relay_set(bool on)
{
    g_relay = on;
}

bool platform_relay_get(void)
{
    return g_relay;
}

void platform_write(const char *data, size_t len)
{
    if (data == NULL || len == 0u) {
        return;
    }

    fwrite(data, 1u, len, stdout);
    fflush(stdout);
}

void platform_poll(void)
{
    /* Host: nothing to poll. */
}

/* --- Mock Flash Implementation --- */

void mock_flash_init(void)
{
    memset(g_mock_flash, 0xFF, sizeof(g_mock_flash));
    g_mock_write_count = 0;
}

uint32_t mock_flash_get_write_count(void)
{
    return g_mock_write_count;
}

err_t platform_flash_read_config(uint32_t slot_index, device_config_t *out)
{
    if (slot_index >= 2u || out == NULL) {
        return ERR_INVALID_ARG;
    }

    uint32_t offset = slot_index * sizeof(device_config_t);
    
    /* Check if erased (all 0xFF) */
    bool empty = true;
    for (size_t i = 0; i < sizeof(device_config_t); ++i) {
        if (g_mock_flash[offset + i] != 0xFF) {
            empty = false;
            break;
        }
    }

    if (empty) {
        return ERR_NOT_FOUND; /* Or specific error code */
    }

    memcpy(out, &g_mock_flash[offset], sizeof(device_config_t));
    return ERR_OK;
}

err_t platform_flash_write_config(uint32_t slot_index, const device_config_t *cfg)
{
    if (slot_index >= 2u || cfg == NULL) {
        return ERR_INVALID_ARG;
    }

    uint32_t offset = slot_index * sizeof(device_config_t);

    /* Simulate erase-before-write requirement roughly */
    /* In real flash, you must erase page first. Here we just overwrite. */
    
    memcpy(&g_mock_flash[offset], cfg, sizeof(device_config_t));
    g_mock_write_count++;

    return ERR_OK;
}
