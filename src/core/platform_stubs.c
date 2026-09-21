#include "core/errors.h"
#include "platform/platform.h"
#include "service/storage.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * WEAK IMPLEMENTATIONS OF PLATFORM INTERFACES.
 *
 * These functions serve as safe fallbacks if the linked target does not provide
 * strong implementations. This allows unit tests to link against climate_core
 * without requiring specific hardware drivers or host abstractions, unless
 * explicitly overridden by a mock or real driver.
 */

__attribute__((weak)) uint32_t platform_millis(void)
{
    return 0u;
}

__attribute__((weak)) void platform_wfi(void)
{
    /* No-op */
}

__attribute__((weak)) void platform_relay_set(bool on)
{
    (void)on;
    /* No-op */
}

__attribute__((weak)) bool platform_relay_get(void)
{
    return false;
}

__attribute__((weak)) void platform_write(const char *data, size_t len)
{
    (void)data;
    (void)len;
    /* No-op: logs are discarded by default in core-only builds */
}

__attribute__((weak)) void platform_poll(void)
{
    /* No-op */
}

/*
 * Storage Interface Stubs.
 * By default, persistent storage is considered unavailable/erroring.
 * Platform layers (Host Mock / STM32 Flash Driver) must override these
 * to enable configuration persistence.
 */
__attribute__((weak)) err_t platform_flash_read_config(
    uint32_t slot_index,
    device_config_t *out)
{
    (void)slot_index;
    if (out != NULL) {
        /* Invalidate magic to force validation failure upstream */
        out->magic = 0u;
    }
    return ERR_NOT_FOUND;
}

__attribute__((weak)) err_t platform_flash_write_config(
    uint32_t slot_index,
    const device_config_t *cfg)
{
    (void)slot_index;
    (void)cfg;
    return ERR_STORAGE;
}

__attribute__((weak)) void platform_wdg_init(uint32_t timeout_ms)
{
    (void)timeout_ms;
    /* No-op for host/tests by default */
}

__attribute__((weak)) void platform_wdg_feed(void)
{
    /* No-op for host/tests by default */
}
