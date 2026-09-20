#ifndef SERVICE_MODBUS_REGS_H
#define SERVICE_MODBUS_REGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Modbus Address Map for Climate Node.
 * All addresses are 0-based internal indices.
 * On wire, they appear as-is in PDU.
 */

enum {
    MB_REG_TEMP_FILT = 0x0000, /* int16_t, 0.01 degC */
    MB_REG_RH        = 0x0001, /* uint16_t, 0.01 % */
    MB_REG_DEW_POINT = 0x0002, /* int16_t, 0.01 degC */
    MB_RELAY_STATE   = 0x0003, /* uint16_t, 0 or 1 */
    MB_DEV_STATUS    = 0x0004, /* uint16_t, ctrl_state_t cast */
    MB_ERR_COUNT     = 0x0005, /* uint16_t, diagnostics.fault_count */
    MB_UPTIME_LO     = 0x0006, /* uint16_t, low 16 bits of uptime_ms */
    MB_UPTIME_HI     = 0x0007, /* uint16_t, high 16 bits of uptime_ms */
    
    /* Configuration registers (RW) */
    MB_CFG_MARGIN_ON  = 0x0100, /* int16_t, 0.01 degC */
    MB_CFG_MARGIN_OFF = 0x0101, /* int16_t, 0.01 degC */
    MB_CFG_SAMPLE_MS  = 0x0102, /* uint16_t, ms */
    
    MB_TOTAL_REGS = 0x0103
};

typedef struct {
    /* Input data from controller/diagnostics */
    int16_t temp_filt_cd;
    uint16_t rh_cp;
    int16_t dew_point_cd;
    bool relay_on;
    uint16_t dev_status;
    uint16_t err_count;
    uint32_t uptime_ms;

    /* Configurable parameters */
    int16_t cfg_margin_on_cd;
    int16_t cfg_margin_off_cd;
    uint16_t cfg_sample_ms;
} modbus_reg_map_t;

void mb_regs_init(modbus_reg_map_t *regs);

bool mb_regs_read(
    const modbus_reg_map_t *regs,
    uint16_t start_addr,
    uint16_t quantity,
    uint8_t *out_data,
    size_t out_capacity);

bool mb_regs_write_single(
    modbus_reg_map_t *regs,
    uint16_t reg_addr,
    uint16_t value);

#endif // SERVICE_MODBUS_REGS_H