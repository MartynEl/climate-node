#include "service/modbus_regs.h"

#include <string.h>

#include "core/controller.h"

void mb_regs_init(modbus_reg_map_t *regs)
{
    if (regs == NULL) {
        return;
    }

    memset(regs, 0, sizeof(*regs));

    /* Defaults matching CONTROLLER_DEFAULTS */
    regs->cfg_margin_on_cd = 500;
    regs->cfg_margin_off_cd = 700;
    regs->cfg_sample_ms = 1000;
}

static void put_be16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
}

bool mb_regs_read(
    const modbus_reg_map_t *regs,
    uint16_t start_addr,
    uint16_t quantity,
    uint8_t *out_data,
    size_t out_capacity)
{
    if (regs == NULL || out_data == NULL) {
        return false;
    }

    if (quantity == 0u || quantity > 125u) {
        return false;
    }

    size_t needed_bytes = (size_t)quantity * 2u;
    if (out_capacity < needed_bytes) {
        return false;
    }

    /* Simple linear scan for skeleton. Optimized later if needed. */
    for (uint16_t i = 0u; i < quantity; ++i) {
        uint16_t addr = start_addr + i;
        uint16_t val = 0u;

        switch (addr) {
        case MB_REG_TEMP_FILT:
            val = (uint16_t)regs->temp_filt_cd;
            break;
        case MB_REG_RH:
            val = regs->rh_cp;
            break;
        case MB_REG_DEW_POINT:
            val = (uint16_t)regs->dew_point_cd;
            break;
        case MB_RELAY_STATE:
            val = regs->relay_on ? 1u : 0u;
            break;
        case MB_DEV_STATUS:
            val = regs->dev_status;
            break;
        case MB_ERR_COUNT:
            val = regs->err_count;
            break;
        case MB_UPTIME_LO:
            val = (uint16_t)(regs->uptime_ms & 0xFFFFu);
            break;
        case MB_UPTIME_HI:
            val = (uint16_t)((regs->uptime_ms >> 16) & 0xFFFFu);
            break;
        case MB_CFG_MARGIN_ON:
            val = (uint16_t)regs->cfg_margin_on_cd;
            break;
        case MB_CFG_MARGIN_OFF:
            val = (uint16_t)regs->cfg_margin_off_cd;
            break;
        case MB_CFG_SAMPLE_MS:
            val = regs->cfg_sample_ms;
            break;
        default:
            /* Illegal address -> return false to trigger exception */
            return false;
        }

        put_be16(&out_data[i * 2u], val);
    }

    return true;
}

bool mb_regs_write_single(
    modbus_reg_map_t *regs,
    uint16_t reg_addr,
    uint16_t value)
{
    if (regs == NULL) {
        return false;
    }

    switch (reg_addr) {
    case MB_CFG_MARGIN_ON:
        regs->cfg_margin_on_cd = (int16_t)value;
        return true;
    case MB_CFG_MARGIN_OFF:
        regs->cfg_margin_off_cd = (int16_t)value;
        return true;
    case MB_CFG_SAMPLE_MS:
        if (value == 0u) {
            return false; /* Invalid config */
        }
        regs->cfg_sample_ms = value;
        return true;
    default:
        /* Read-only or illegal address */
        return false;
    }
}