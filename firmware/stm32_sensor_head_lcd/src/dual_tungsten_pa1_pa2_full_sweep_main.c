#include "main.h"
#include "sys.h"
#include <stdint.h>

/*
 * One-shot synchronized dual-tungsten test.
 *
 * PA1/A1 = TIM5_CH2
 * PA2/A2 = TIM5_CH3
 *
 * Both channels execute 0 -> 100% in 1 s and immediately 100% -> 0 in
 * 1 s.  They then remain disabled indefinitely.  Full duty is safe only
 * when each external supply is set to its lamp rating.
 */
#define PWM_PORT                   GPIOA
#define PWM_PINS                   (GPIO_PIN_1 | GPIO_PIN_2)

#define CORE_CLOCK_HZ              400000000u
#define TIM5_CLOCK_HZ              200000000u
#define PWM_FREQUENCY_HZ           443u
#define PWM_PERIOD_TICKS           ((TIM5_CLOCK_HZ + (PWM_FREQUENCY_HZ / 2u)) / PWM_FREQUENCY_HZ)
#define CONTROL_RATE_HZ            1000u
#define CONTROL_PERIOD_CYCLES      (CORE_CLOCK_HZ / CONTROL_RATE_HZ)
#define RAMP_FRAMES                1000u

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void outputs_off(void)
{
    TIM5->CCR2 = 0u;
    TIM5->CCR3 = 0u;
    TIM5->EGR = TIM_EGR_UG;
}

void Error_Handler(void)
{
    outputs_off();
    TIM5->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC3E);
    __disable_irq();
    while (1) {}
}

static inline void wait_cycles(uint32_t start, uint32_t cycles)
{
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void pwm_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();

    HAL_GPIO_WritePin(PWM_PORT, PWM_PINS, GPIO_PIN_RESET);
    gpio.Pin = PWM_PINS;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF2_TIM5;
    HAL_GPIO_Init(PWM_PORT, &gpio);

    TIM5->CR1 = 0u;
    TIM5->CR2 = 0u;
    TIM5->SMCR = 0u;
    TIM5->DIER = 0u;
    TIM5->PSC = 0u;
    TIM5->ARR = PWM_PERIOD_TICKS - 1u;
    TIM5->CCR2 = 0u;
    TIM5->CCR3 = 0u;
    TIM5->CCMR1 = (TIM5->CCMR1 & ~(TIM_CCMR1_OC2M | TIM_CCMR1_CC2S)) |
                  TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2PE;
    TIM5->CCMR2 = (TIM5->CCMR2 & ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S)) |
                  TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
    TIM5->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC2P | TIM_CCER_CC2NP |
                    TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP);
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0u;
    TIM5->CCER |= TIM_CCER_CC2E | TIM_CCER_CC3E;
    TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static uint16_t smoothstep_q16(uint32_t index)
{
    uint64_t x;
    uint64_t x2;
    uint64_t y;

    if (index >= RAMP_FRAMES - 1u) {
        return 65535u;
    }
    x = ((uint64_t)index * 65535u) / (RAMP_FRAMES - 1u);
    x2 = (x * x + 32767u) / 65535u;
    y = (x2 * (196605u - 2u * x) + 32767u) / 65535u;
    return (uint16_t)(y > 65535u ? 65535u : y);
}

static void set_both(uint16_t duty)
{
    uint32_t period = TIM5->ARR + 1u;
    uint32_t compare = (uint32_t)(((uint64_t)duty * period + 32767u) / 65535u);

    TIM5->CCR2 = compare;
    TIM5->CCR3 = compare;
}

static void wait_control_deadline(uint32_t *next_update)
{
    *next_update += CONTROL_PERIOD_CYCLES;
    while ((int32_t)(DWT->CYCCNT - *next_update) < 0) {
    }
}

static void run_sweep(void)
{
    uint32_t next_update = DWT->CYCCNT;

    for (uint32_t frame = 0u; frame < RAMP_FRAMES; frame++) {
        set_both(smoothstep_q16(frame));
        wait_control_deadline(&next_update);
    }
    for (uint32_t frame = 0u; frame < RAMP_FRAMES; frame++) {
        set_both(smoothstep_q16(RAMP_FRAMES - 1u - frame));
        wait_control_deadline(&next_update);
    }
    outputs_off();
    TIM5->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC3E);
}

int main(void)
{
    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u);
    dwt_init();
    pwm_init();
    outputs_off();
    run_sweep();

    /* Permanent cooling state after the single 2 s waveform. */
    while (1) {
        __WFI();
    }
}
