#include "platform/stm32f1/platform_stm32f1.h"
#include "platform/platform.h"
#include "service/storage.h"
#include "bsp/stm32f1/uart.h"
#include "bsp/i2c.h"
#include "core/controller.h"

#include <stddef.h>
#include <stdint.h>

extern volatile uint32_t SystemTicks;

/* --- Аппаратные адреса регистров STM32F103 --- */
#define RCC_BASE             0x40021000UL
#define RCC_APB1ENR          (*(volatile uint32_t *)(RCC_BASE + 0x1運行))
#define RCC_APB2ENR          (*(volatile uint32_t *)(RCC_BASE + 0x18UL))
#define RCC_CSR              (*(volatile uint32_t *)(RCC_BASE + 0x24UL))

#define RCC_APB2ENR_IOPCEN  (1UL << 4)
#define RCC_APB2ENR_IOPBEN  (1UL << 3)

#define GPIOC_BASE          0x40011000UL
#define GPIOC_CRH           (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))
#define GPIOC_BSRR          (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_BRR           (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))

#define GPIOB_BASE           0x40010C00UL
#define GPIOB_CRL            (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_BSRR           (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_BRR           (*(volatile uint32_t *)(GPIOB_BASE + 0x10UL))

#define SYSTICK_BASE        0xE000E010UL
#define SYSTICK_CTRL        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL         (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))

#define SYSTICK_CTRL_ENABLE    (1UL << 0)
#define SYSTICK_CTRL_TICKINT   (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE (1UL << 2)

#define PLATFORM_CLOCK_HZ 8000000UL
#define SYSTICK_RELOAD_1MS ((PLATFORM_CLOCK_HZ / 1000UL) - 1UL)

/* Конфигурация секторов Flash (64KB) */
#define FLASH_CONFIG_A_ADDR 0x0800F800UL
#define FLASH_CONFIG_B_ADDR 0x0800FC00UL

/* --- Регистры управления питанием PWR и BKP --- */
#define PWR_BASE             0x40007000UL
#define PWR_CR               (*(volatile uint32_t *)(PWR_BASE + 0x00UL))
#define PWR_CR_PVDE          (1UL << 4)  // PVD Enable
#define PWR_CR_PLS_2V9       (5UL << 5)  // Порог детекции Brown-out = 2.9 В

#define BKP_BASE             0x40006C00UL
#define BKP_DR1              (*(volatile uint32_t *)(BKP_BASE + 0x04UL))

#define NVIC_ISER0           (*(volatile uint32_t *)0xE000E100UL)
#define PVD_IRQ_BIT          (1UL << 1)  // PVD — вектор прерывания 1 в NVIC

/* --- Регистры сторожевого таймера IWDG --- */
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

extern controller_t g_app_controller; // Ссылка на FSM ядра из main_mcu.c
static bool g_relay_state = false;

/**
 * Чтение причины аппаратного сброса
 */
reset_cause_t platform_reset_cause_get(void)
{
    uint32_t csr = RCC_CSR;

    // Сбрасываем флаги RCC для следующего цикла работы девайса
    RCC_CSR |= (1UL << 24); // RMVF: Remove Reset Flags

    if ((csr & (1UL << 29)) != 0u) return RESET_CAUSE_WATCHDOG;  // IWDGRSTF
    if ((csr & (1UL << 28)) != 0u) return RESET_CAUSE_SOFTWARE;  // SFTRSTF
    if ((csr & (1UL << 26)) != 0u) return RESET_CAUSE_EXTERNAL;  // PINRSTF
    if ((csr & (1UL << 27)) != 0u) return RESET_CAUSE_POWER_ON;  // PORRSTF

    return RESET_CAUSE_UNKNOWN;
}

/**
 * Инициализация аппаратного детектора Brown-out (PVD)
 */
void platform_pvd_init(void)
{
    // Включаем тактирование PWR и бэкап-домена BKP в APB1
    *(volatile uint32_t *)(0x40021000UL + 0x1CUL) |= (1UL << 28) | (1UL << 27);
    
    // Задаем порог 2.9V и активируем внутренний компаратор питания
    PWR_CR = PWR_CR_PLS_2V9 | PWR_CR_PVDE;

    // Конфигурируем линию EXTI 16 (внутренний триггер прерывания PVD)
    *(volatile uint32_t *)0x40010400UL |= (1UL << 16); // EXTI_IMR
    *(volatile uint32_t *)0x40010408UL |= (1UL << 16); // EXTI_RTSR: сработка по падению напряжения

    NVIC_ISER0 |= PVD_IRQ_BIT; // Активируем прерывание в NVIC
}

/**
 * Сохранение статуса в Backup RAM (сверхбыстрая регистровая запись)
 */
void platform_emergency_backup_save(const void *data, size_t len)
{
    if (data == NULL || len == 0) return;
    
    // Разрешаем запись в Backup область (бит DBP в PWR_CR)
    PWR_CR |= (1UL << 8); 

    uint8_t *byte_ptr = (uint8_t *)data;
    BKP_DR1 = (uint32_t)(*byte_ptr);
}

/**
 * Восстановление бэкапа аварий при старте прошивки
 */
bool platform_emergency_backup_load(void *out_data, size_t len)
{
    if (out_data == NULL || len == 0) return false;
    
    uint8_t *byte_ptr = (uint8_t *)out_data;
    *byte_ptr = (uint8_t)(BKP_DR1 & 0xFFu);
    return true;
}

/**
 * ИСР: Высокоприоритетный аварийный прерывание детектора Brown-out
 */
void PVD_IRQHandler(void)
{
    // Экстренная остановка логики и сброс силовых реле
    ctrl_force_emergency_shutdown(&g_app_controller);

    // Сброс флага линии EXTI
    *(volatile uint32_t *)0x40010414UL = (1UL << 16); // EXTI_PR
}

/**
 * Полный запуск и конфигурирование платформы STM32F1
 */
void stm32f1_platform_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPBEN;

    // PC13 — Выход общего назначения (Встроенный LED)
    uint32_t crh = GPIOC_CRH;
    crh &= ~(0xFUL << 20);
    crh |= (0x2UL << 20);
    GPIOC_CRH = crh;
    GPIOC_BSRR = (1UL << 13);

    // PB0 — Настройка направления RS-485 DE/RE
    uint32_t crl = GPIOB_CRL;
    crl &= ~(0xFUL << 0);      
    crl |= (0x1UL << 0);       // Выход Push-Pull, 10МГц
    GPIOB_CRL = crl;
    GPIOB_BRR = (1UL << 0);    // Изначально RE активен (низкий уровень)

    /* Системный таймер миллисекундных тиков */
    SYSTICK_LOAD = SYSTICK_RELOAD_1MS;
    SYSTICK_VAL = 0u;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;

    // УДАЛИЛИ uart1_init(); — она больше не нужна здесь

    // Настраиваем пин RS-485 для нового драйвера
    uart1_set_rs485_pin(GPIOB_BASE, 0); 

    // Активируем компаратор PVD защиты по питанию при старте
    platform_pvd_init();
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
    IWDG_KR = IWDG_KR_KEY_UNLOCK;
    IWDG_PR = 4u; // Прескалер 64

    while ((IWDG_SR & IWDG_SR_PVU_BIT) != 0u) {}

    uint32_t reload_val = (timeout_ms * 5u) / 8u;
    if (reload_val > 4095u) reload_val = 4095u;
    if (reload_val < 1u)  reload_val = 1u;

    IWDG_RLR = reload_val;

    while ((IWDG_SR & IWDG_SR_RVU_BIT) != 0u) {}
    IWDG_KR = IWDG_KR_KEY_START;
}

void platform_wdg_feed(void)
{
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
    if (data == NULL || len == 0u) return;
    // Используем наш новый асинхронный TX DMA для отправки логов в терминал
    (void)uart1_tx_dma_async((const uint8_t *)data, len);
}

void platform_poll(void)
{
    // В серийной DMA архитектуре поллинг пустой — за нас работает железо!
}

err_t platform_flash_read_config(uint32_t slot_index, device_config_t *out)
{
    if (out == NULL) return ERR_INVALID_ARG;

    uint32_t addr = (slot_index == 0u) ? FLASH_CONFIG_A_ADDR : FLASH_CONFIG_B_ADDR;
    const uint32_t *flash_ptr = (const uint32_t *)addr;

    uint8_t *dst = (uint8_t *)out;
    for (size_t i = 0; i < sizeof(device_config_t); i += 4) {
        uint32_t val = *flash_ptr++;
        dst[i] = (uint8_t)(val & 0xFF);
        dst[i+1] = (uint8_t)((val >> 8) & 0xFF);
        dst[i+2] = (uint8_t)((val >> 16) & 0xFF);
        dst[i+3] = (uint8_t)((val >> 24) & 0xFF);
    }

    bool empty = true;
    for (size_t i = 0; i < sizeof(device_config_t); ++i) {
        if (dst[i] != 0xFF) {
            empty = false;
            break;
        }
    }
    if (empty) return ERR_NOT_FOUND;
    return ERR_OK;
}

err_t platform_flash_write_config(uint32_t slot_index, const device_config_t *cfg)
{
    (void)slot_index;
    (void)cfg;
    return ERR_OK; 
}
