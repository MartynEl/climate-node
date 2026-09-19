#include "platform/platform.h"
#include "platform/host/platform_host.h"

static uint32_t g_now_ms = 0;
static bool g_relay = false;

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