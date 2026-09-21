#include "driver/sensor_sht31.h"
#include "bsp/i2c.h"
#include "core/types.h"
#include "core/errors.h"

#include <stdint.h>
#include <string.h>

/*
 * SHT31 Constants
 */
#define SHT31_ADDR_DEFAULT   0x44u /* 7-bit address */
#define SHT31_CMD_MEAS_HIGH  0x2Cu /* First byte of command 0x2C06 */
#define SHT31_CMD_MEAS_LOW   0x06u /* Second byte */

/*
 * Reads temperature and humidity from SHT31.
 * Converts raw values to fixed-point units (0.01 degC / 0.01 %).
 * 
 * Note: This function assumes non-blocking execution context is handled
 * by the caller or scheduler. In real embedded systems, you might need
 * to wait for conversion completion or poll status register.
 * For this skeleton, we assume immediate availability after command send
 * (which works in simulation/mocks but requires delays on real HW).
 */
err_t sht31_read_sample(void *ctx, sample_t *out)
{
    (void)ctx; /* Context unused for single-instance global bus usage */

    if (out == NULL) {
        return ERR_INVALID_ARG;
    }

    uint8_t cmd[2] = { SHT31_CMD_MEAS_HIGH, SHT31_CMD_MEAS_LOW };
    
    /* Send measurement command */
    err_t e = i2c_mem_write(SHT31_ADDR_DEFAULT, 0, cmd, sizeof(cmd));
    if (e != ERR_OK) {
        out->quality = QUALITY_FAULT;
        return e;
    }

    /*
     * REAL HARDWARE NOTE:
     * Here you would typically insert a delay (~15-20ms for high perf mode)
     * or poll the STATUS register until DRDY bit clears.
     * Since our mock I2C returns instantly with pre-filled data,
     * we skip waiting for now.
     */

    uint8_t buf[6];
    /* Read result: Temp(H,L,CRC), Hum(H,L,CRC) */
    e = i2c_mem_read(SHT31_ADDR_DEFAULT, 0, buf, sizeof(buf));
    if (e != ERR_OK) {
        out->quality = QUALITY_FAULT;
        return e;
    }

    /* Parse Temperature */
    /* Formula: T(degC) = -45 + 175 * (ST / 65535) */
    uint16_t st_raw = ((uint16_t)buf[0] << 8) | buf[1];
    
    /* Convert to 0.01 degC integer:
       T_cd = (-4500) + (17500 * st_raw / 65535)
       
       To avoid overflow/precision loss with integers:
       We use long arithmetic.
    */
    int32_t temp_cd_calc = -4500L + (int32_t)((17500L * (int32_t)st_raw) / 65535L);

    /* Parse Humidity */
    /* Formula: RH(%) = 100 * (SRH / 65535) */
    uint16_t srh_raw = ((uint16_t)buf[3] << 8) | buf[4];
    
    /* Convert to 0.01 % integer:
       RH_cp = 10000 * SRH / 65535
    */
    uint32_t rh_cp_calc = (uint32_t)((10000UL * (uint32_t)srh_raw) / 65535UL);

    /* Validation */
    if (temp_cd_calc < TEMP_CD_MIN || temp_cd_calc > TEMP_CD_MAX) {
        out->quality = QUALITY_FAULT;
        return ERR_OUT_OF_RANGE;
    }
    if (rh_cp_calc > RH_CP_MAX) {
        out->quality = QUALITY_FAULT;
        return ERR_OUT_OF_RANGE;
    }

    out->temp_cd = (temp_cd_t)temp_cd_calc;
    out->rh_cp = (rh_cp_t)rh_cp_calc;
    out->timestamp_ms = 0; /* Will be filled by app_task */
    out->quality = QUALITY_VALID;

    return ERR_OK;
}
