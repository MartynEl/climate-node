#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

uint32_t platform_millis(void);
void platform_wfi(void);
void platform_watchdog_feed(void);

void platform_relay_set(bool on);
bool platform_relay_get(void);

#endif // PLATFORM_PLATFORM_H