#include "platform/stm32f1/platform_stm32f1.h"
#include "platform/platform.h"

#include <stddef.h>
#include <stdint.h>

extern volatile uint32_t SystemTicks;

#define RCC_BASE           0x40021000UL
#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18UL))
#define RCC_APB2ENR_IOPCEN (1UL << 4)

#define GPIOC_BASE  0x40011000UL
#define GPIOC_CRH   (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))
#define GPIOC_BSRR  (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_BRR   (*(volatile uint32_t *)(GPIOC_BASE + 0x10UL))

#define SYSTICK_BASE             0xE000E010UL
#define SYSTICK_CTRL             (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD             (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL              (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))

#define SYSTICK_CTRL_ENABLE      (1UL << 0)
#define SYSTICK_CTRL_TICKINT     (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE   (1UL << 2)

#define PLATFORM_CLOCK_HZ 8000000UL
#define SYSTICK_RELOAD_1MS ((PLATFORM_CLOCK_HZ / 1000UL) - 1UL)

static bool g_relay_state = false;

void stm32f1_platform_init(void)
{
    /*
     * Enable GPIOC clock.
     * PC13 is commonly used as onboard LED on Blue Pill-like boards.
     * LED is usually active-low.
     */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /*
     * Configure PC13 as general purpose output, 2 MHz.
     * CRH controls pins 8..15.
     * Pin 13 occupies bits 20..23.
     */
    uint32_t crh = GPIOC_CRH;
    crh &= ~(0xFUL << 20);
    crh |= (0x2UL << 20);
    GPIOC_CRH = crh;

    /* Turn LED off initially: set PC13 high. */
    GPIOC_BSRR = (1UL << 13);

    /*
     * SysTick: 1 ms tick from default HSI 8 MHz.
     * This is intentionally minimal. Proper clock configuration comes later.
     */
    SYSTICK_LOAD = SYSTICK_RELOAD_1MS;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;
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
    /* TODO: IWDG in a later commit. */
}

void platform_relay_set(bool on)
{
    g_relay_state = on;

    /*
     * Active-low LED/relay convention on many STM32F103 boards:
     *   on  -> drive PC13 low
     *   off -> drive PC13 high
     */
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
    (void)data;
    (void)len;

    /*
     * TODO: non-blocking UART TX logger in a later commit.
     * For the build skeleton, logging is intentionally no-op on MCU.
     */
}
