#ifndef PLATFORM_PLATFORM_H
#define PLATFORM_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t platform_millis(void);
void platform_wfi(void);

void platform_relay_set(bool on);
bool platform_relay_get(void);

void platform_write(const char *data, size_t len);
void platform_poll(void);

/**
 * Initialize Independent Watchdog.
 * @param timeout_ms Timeout in milliseconds before reset.
 */
void platform_wdg_init(uint32_t timeout_ms);

/**
 * Feed the watchdog. Should be called periodically.
 * In ticket-based systems, call only if all critical tasks completed.
 */
void platform_wdg_feed(void);

#endif // PLATFORM_PLATFORM_H
