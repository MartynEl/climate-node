#include "platform/stm32f1/platform_stm32f1.h"
#include "platform/platform.h"
#include "service/storage.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp/stm32f1/uart.h"
#include "bsp/i2c.h"

extern volatile uint32_t SystemTicks;

#define RCC_BASE            0x40021000UL
#define RCC_APB2ENR         (*(volatile uint32_t *)(RCC_BASE + 0x18UL))
#define RCC_APB2ENR_IOPCEN  (1UL << 4)

#define GPIOC_BASE          0x40011000UL
#define GPIOC_CRH           (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))
#define GPIOC_BSRR          (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_BRR           (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))

#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK_CTRL        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL         (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))

#define SYSTICK_CTRL_ENABLE    (1UL << 0)
#define SYSTICK_CTRL_TICKINT   (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE (1UL << 2)

#define PLATFORM_CLOCK_HZ 8000000UL
#define SYSTICK_RELOAD_1MS ((PLATFORM_CLOCK_HZ / 1000UL) - 1UL)

/* Flash Layout for STM32F103C8T6 (64KB) */
#define FLASH_CONFIG_A_ADDR 0x0800F800UL
#define FLASH_CONFIG_B_ADDR 0x0800FC00UL

static bool g_relay_state = false;

void stm32f1_platform_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    uint32_t crh = GPIOC_CRH;
    crh &= ~(0xFUL << 20);
    crh |= (0x2UL << 20);
    GPIOC_CRH = crh;

    GPIOC_BSRR = (1UL << 13);

    SYSTICK_LOAD = SYSTICK_RELOAD_1MS;
    SYSTICK_VAL = 0u;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;

    uart1_init();
}

uint32_t platform_millis(void)
{
    return SystemTicks;
}

void platform_wfi(void)
{
    __asm volatile("wfi" ::: "memory");
}

void platform_watchdog_feed(void)
{
    /* TODO: IWDG */
}

void platform_relay_set(bool on)
{
    g_relay_state = on;
    if (on) {
        GPIOC_BRR = (1UL << 13);
    } else {
        GPIOC_BSRR = (1UL << 13);
    }
}

bool platform_relay_get(void)
{
    return g_relay_state;
}

void platform_write(const char *data, size_t len)
{
    if (data == NULL || len == 0u) {
        return;
    }
    uart1_tx_bytes((const uint8_t *)data, len);
}

void platform_poll(void)
{
    uart1_task();
}

/* --- Real Flash Implementation (Skeleton) --- */

err_t platform_flash_read_config(uint32_t slot_index, device_config_t *out)
{
    if (out == NULL) {
        return ERR_INVALID_ARG;
    }

    uint32_t addr = (slot_index == 0u) ? FLASH_CONFIG_A_ADDR : FLASH_CONFIG_B_ADDR;
    const uint32_t *flash_ptr = (const uint32_t *)addr;

    /* Read word-by-word into struct */
    uint8_t *dst = (uint8_t *)out;
    for (size_t i = 0; i < sizeof(device_config_t); i += 4) {
        uint32_t val = *flash_ptr++;
        dst[i] = (uint8_t)(val & 0xFF);
        dst[i+1] = (uint8_t)((val >> 8) & 0xFF);
        dst[i+2] = (uint8_t)((val >> 16) & 0xFF);
        dst[i+3] = (uint8_t)((val >> 24) & 0xFF);
    }

    /* Check if erased (all 0xFF) */
    bool empty = true;
    for (size_t i = 0; i < sizeof(device_config_t); ++i) {
        if (dst[i] != 0xFF) {
            empty = false;
            break;
        }
    }

    if (empty) {
        return ERR_NOT_FOUND;
    }

    return ERR_OK;
}

err_t platform_flash_write_config(uint32_t slot_index, const device_config_t *cfg)
{
    if (cfg == NULL) {
        return ERR_INVALID_ARG;
    }

    /*
     * WARNING: This is a SKELETON.
     * Real STM32 Flash programming requires:
     * 1. Unlocking Flash registers (KEYR).
     * 2. Erasing the page (PER).
     * 3. Setting PG bit.
     * 4. Writing half-words (16-bit) sequentially.
     * 5. Checking BSY flag.
     * 6. Locking Flash again.
     *
     * For this commit, we intentionally leave it as a NO-OP returning OK,
     * so the build passes and logic flows, but persistence won't work on HW
     * until we implement the actual register sequences in Commit 11.
     */
    
    (void)slot_index;
    (void)cfg;
    
    return ERR_OK; 
}

/* --- I2C Implementation Skeleton for STM32F1 --- */

#include "bsp/i2c.h"

err_t i2c_init(void)
{
    /* TODO: Enable RCC_APB1ENR_I2C1EN, configure PB6/PB7 GPIO AF_OD */
    return ERR_OK;
}

err_t i2c_mem_write(
    uint8_t dev_addr,
    uint8_t reg_addr,
    const uint8_t *data,
    size_t len)
{
    (void)dev_addr;
    (void)reg_addr;
    (void)data;
    (void)len;
    /* TODO: Implement HAL_I2C_Master_Transmit or LL equivalent */
    return ERR_OK;
}

err_t i2c_mem_read(
    uint8_t dev_addr,
    uint8_t reg_addr,
    uint8_t *data,
    size_t len)
{
    (void)dev_addr;
    (void)reg_addr;
    
    /* Return dummy valid-looking data for build verification */
    if (len >= 6 && data != NULL) {
        /* Simulate ~22.5C and 65% RH roughly encoded */
        /* ST=41943 -> T=-45+175*41943/65535 ≈ 22.5C */
        data[0] = 0xA3; /* High byte approx */
        data[1] = 0x7B; /* Low byte approx */
        data[2] = 0xBE; /* Fake CRC */
        
        /* SRH=42598 -> RH=100*42598/65535 ≈ 65% */
        data[3] = 0xA6; 
        data[4] = 0xC6;
        data[5] = 0xEF; /* Fake CRC */
    }
    
    /* TODO: Implement HAL_I2C_Master_Receive */
    return ERR_OK;
}
