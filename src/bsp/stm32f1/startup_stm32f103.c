#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern int main(void);

volatile uint32_t SystemTicks = 0u;

void Default_Handler(void)
{
    for (;;) {
    }
}

void Reset_Handler(void);

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVCall_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));

void SysTick_Handler(void)
{
    SystemTicks++;
}

/*
 * Defined in src/bsp/stm32f1/uart.c.
 */
void USART1_IRQHandler(void);

/*
 * Cortex-M3 vector table:
 *   0..15  core exceptions
 *   16..   external IRQs
 *
 * STM32F103 USART1_IRQn = 37, therefore vector index = 16 + 37 = 53.
 */
__attribute__((section(".isr_vector"), used))
const uint32_t g_isr_vector[] = {
    [0]  = (uint32_t)&_estack,
    [1]  = (uint32_t)Reset_Handler,
    [2]  = (uint32_t)NMI_Handler,
    [3]  = (uint32_t)HardFault_Handler,
    [4]  = (uint32_t)MemManage_Handler,
    [5]  = (uint32_t)BusFault_Handler,
    [6]  = (uint32_t)UsageFault_Handler,

    [7]  = 0u,
    [8]  = 0u,
    [9]  = 0u,
    [10] = 0u,

    [11] = (uint32_t)SVCall_Handler,
    [12] = (uint32_t)DebugMon_Handler,
    [13] = 0u,
    [14] = 0u,
    [15] = (uint32_t)PendSV_Handler,
    [16] = (uint32_t)SysTick_Handler,

    [17 ... 52] = (uint32_t)Default_Handler,
    [53]        = (uint32_t)USART1_IRQHandler,
    [54 ... 67] = (uint32_t)Default_Handler
};

void Reset_Handler(void)
{
    /*
     * Set vector table offset register to flash base.
     */
    volatile uint32_t *vtor = (volatile uint32_t *)0xE000ED08UL;
    *vtor = 0x08000000UL;

    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0u;
    }

    (void)main();

    for (;;) {
    }
}
