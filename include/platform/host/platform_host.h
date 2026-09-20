#ifndef PLATFORM_HOST_PLATFORM_HOST_H
#define PLATFORM_HOST_PLATFORM_HOST_H

#include <stdint.h>

void platform_host_tick(uint32_t ms);

/* Mock Flash API for host testing */
void mock_flash_init(void);
uint32_t mock_flash_get_write_count(void);

#endif // PLATFORM_HOST_PLATFORM_HOST_H
