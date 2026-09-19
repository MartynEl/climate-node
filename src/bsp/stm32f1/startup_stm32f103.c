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

__attribute__((section(".isr_vector"), used))
const uint32_t g_isr_vector[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)NMI_Handler,
    (uint32_t)HardFault_Handler,
    (uint32_t)MemManage_Handler,
    (uint32_t)BusFault_Handler,
    (uint32_t)UsageFault_Handler,

    0,
    0,
    0,
    0,

    (uint32_t)SVCall_Handler,
    (uint32_t)DebugMon_Handler,
    0,
    0,
    (uint32_t)PendSV_Handler,
    (uint32_t)SysTick_Handler
};

void Reset_Handler(void)
{
    /*
     * Set vector table offset register to flash base.
     * For STM32F103 with this linker script, code starts at 0x08000000.
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
