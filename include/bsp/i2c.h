#ifndef BSP_I2C_H
#define BSP_I2C_H

#include <stddef.h>
#include <stdint.h>

#include "core/errors.h"

/*
 * Low-level I2C Bus Abstraction.
 * Implemented by platform-specific drivers (STM32, RISC-V, Host Mock).
 */

/**
 * Initialize I2C peripheral (clocks, GPIO, registers).
 */
err_t i2c_init(void);

/**
 * Write data to a device register/memory address.
 * 
 * @param dev_addr 7-bit slave address.
 * @param reg_addr Starting memory/register address on the slave.
 * @param data Pointer to source buffer.
 * @param len Number of bytes to write.
 * @return ERR_OK on success, error code otherwise.
 */
err_t i2c_mem_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, size_t len);

/**
 * Read data from a device register/memory address.
 * 
 * @param dev_addr 7-bit slave address.
 * @param reg_addr Starting memory/register address on the slave.
 * @param data Destination buffer.
 * @param len Number of bytes to read.
 * @return ERR_OK on success, error code otherwise.
 */
err_t i2c_mem_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);

#endif // BSP_I2C_H
