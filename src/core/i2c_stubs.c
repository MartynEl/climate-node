#include "bsp/i2c.h"
#include "core/errors.h"

#include <stddef.h>
#include <stdint.h>

/*
 * WEAK IMPLEMENTATIONS OF I2C BUS.
 * These allow unit tests and core logic compilation without hardware drivers.
 * Platform layers must provide strong overrides for actual operation.
 */

__attribute__((weak)) err_t i2c_init(void)
{
    return ERR_NOT_FOUND; /* No I2C available by default */
}

__attribute__((weak)) err_t i2c_mem_write(
    uint8_t dev_addr,
    uint8_t reg_addr,
    const uint8_t *data,
    size_t len)
{
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    return ERR_COMM; /* Communication failure */
}

__attribute__((weak)) err_t i2c_mem_read(
    uint8_t dev_addr,
    uint8_t reg_addr,
    uint8_t *data,
    size_t len)
{
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    return ERR_COMM; /* Communication failure */
}
