#include "delay.h"

void delay_init(u16 SYSCLK)
{
    (void)SYSCLK;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_ms(u16 nms)
{
    HAL_Delay(nms);
}

void delay_us(u32 nus)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000U) * nus;
    while ((uint32_t)(DWT->CYCCNT - start) < ticks) {
        __NOP();
    }
}
