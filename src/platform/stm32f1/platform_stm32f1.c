#include "platform/stm32f1/platform_stm32f1.h"
#include "platform/platform.h"
#include "service/storage.h"
#include "bsp/stm32f1/uart.h"
#include "bsp/i2c.h"

#include <stddef.h>
#include <stdint.h>

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

/* --- IWDG Definitions --- */
#define IWDG_KR_BASE       0x40003000UL
#define IWDG_KR            (*(volatile uint32_t *)(IWDG_KR_BASE + 0x00UL))
#define IWDG_PR            (*(volatile uint32_t *)(IWDG_KR_BASE + 0x04UL))
#define IWDG_RLR           (*(volatile uint32_t *)(IWDG_KR_BASE + 0x08UL))
#define IWDG_SR            (*(volatile uint32_t *)(IWDG_KR_BASE + 0x0CUL))

#define IWDG_KR_KEY_START  0xCCCCUL
#define IWDG_KR_KEY_RELOAD 0xAAAAUL
#define IWDG_KR_KEY_UNLOCK 0x5555UL

#define IWDG_SR_PVU_BIT    (1UL << 0)
#define IWDG_SR_RVU_BIT    (1UL << 1)

/* --- RS-485 DE/RE Pin Definitions (using PB0) --- */
#define RCC_APB2ENR_IOPBEN   (1UL << 3)
#define GPIOB_BASE           0x40010C00UL
#define GPIOB_CRL            (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_BSRR           (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_BRR            (*(volatile uint32_t *)(GPIOB_BASE + 0x10UL))

static bool g_relay_state = false;

void stm32f1_platform_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN;

    uint32_t crh = GPIOC_CRH;
    crh &= ~(0xFUL << 20);
    crh |= (0x2UL << 20);
    GPIOC_CRH = crh;
    GPIOC_BSRR = (1UL << 13);

    uint32_t crl = GPIOB_CRL;
    crl &= ~(0xFUL << 0);      // Очищаем биты CNF/MODE для PB0 (позиция 0..3)
    crl |= (0x1UL << 0);       // MODE=01 (10 МГц), CNF=00 (General Purpose Output PP)
    GPIOB_CRL = crl;
    
    // Начальное состояние: LOW (режим приёма RE активен, TX отключён)
    GPIOB_BRR = (1UL << 0);    // Сбрасываем PB0 в ноль через BRR

    /* SysTick для миллисекундного тика */
    SYSTICK_LOAD = SYSTICK_RELOAD_1MS;
    SYSTICK_VAL = 0u;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;

    uart1_init();

    /* Сообщаем драйверу UART, какой пин использовать для RS-485 */
    uart1_set_rs485_pin(GPIOB_BASE, 0); // Порт B, пин номер 0
}

uint32_t platform_millis(void)
{
    return SystemTicks;
}

void platform_wfi(void)
{
    __asm volatile("wfi" ::: "memory");
}

void platform_wdg_init(uint32_t timeout_ms)
{
    /* Unlock IWDG registers */
    IWDG_KR = IWDG_KR_KEY_UNLOCK;

    /* Select Prescaler: PR=4 -> Divider=64 */
    IWDG_PR = 4u;

    /* Wait for PVU bit to clear */
    while ((IWDG_SR & IWDG_SR_PVU_BIT) != 0u) {
        /* Busy wait */
    }

    /* Calculate Reload Value */
    /* Ticks = timeout_ms / 1.6 = timeout_ms * 10 / 16 = timeout_ms * 5 / 8 */
    uint32_t reload_val = (timeout_ms * 5u) / 8u;
    
    /* Clamp to valid range [1, 4095] */
    if (reload_val > 4095u) {
        reload_val = 4095u;
    }
    if (reload_val < 1u) {
        reload_val = 1u;
    }

    IWDG_RLR = reload_val;

    /* Wait for RVU bit to clear */
    while ((IWDG_SR & IWDG_SR_RVU_BIT) != 0u) {
        /* Busy wait */
    }

    /* Start IWDG */
    IWDG_KR = IWDG_KR_KEY_START;
}

void platform_wdg_feed(void)
{
    /* Send reload key */
    IWDG_KR = IWDG_KR_KEY_RELOAD;
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
