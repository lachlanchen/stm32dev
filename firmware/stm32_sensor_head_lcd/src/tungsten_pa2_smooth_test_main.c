#include "main.h"
#include "sys.h"
#include <stdint.h>

/*
 * Independent single-run 12 V tungsten test.
 * PA2/A2 uses TIM5_CH3 at 443 Hz, leaving TIM2 available for PA3 pixels.
 */
#define LAMP_PORT                  GPIOA
#define LAMP_PIN                   GPIO_PIN_2
#define PIXEL_PORT                 GPIOA
#define PIXEL_PIN                  GPIO_PIN_3

#define CORE_CLOCK_HZ              400000000u
#define TIM5_CLOCK_HZ              200000000u
#define PWM_FREQUENCY_HZ           443u
#define PWM_PERIOD_TICKS           ((TIM5_CLOCK_HZ + (PWM_FREQUENCY_HZ / 2u)) / PWM_FREQUENCY_HZ)
#define CONTROL_RATE_HZ            1000u
#define CONTROL_PERIOD_CYCLES      (CORE_CLOCK_HZ / CONTROL_RATE_HZ)

/* Previous visible range was approximately 100..255 on the Arduino scale. */
#define DUTY_START                 25700u
#define DUTY_MAX                   65535u
#define RAMP_UP_MS                 3000u
#define RAMP_DOWN_MS               3000u
#define FADE_OFF_MS                500u
#define STARTUP_OFF_MS             1000u

#define PIXEL_OFF_BITS             64u
#define PIXEL_T0H_CYCLES           120u
#define PIXEL_BIT_CYCLES           500u
#define PIXEL_RESET_US             300u

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    TIM5->CCR3 = 0u;
    TIM5->CCER &= ~TIM_CCER_CC3E;
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    __disable_irq();
    while (1) {}
}

static inline void wait_cycles(uint32_t start, uint32_t cycles)
{
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {
        __NOP();
    }
}

static void hold_us(uint32_t microseconds)
{
    while (microseconds != 0u) {
        uint32_t chunk = microseconds > 1000u ? 1000u : microseconds;
        uint32_t start = DWT->CYCCNT;
        wait_cycles(start, (CORE_CLOCK_HZ / 1000000u) * chunk);
        microseconds -= chunk;
    }
}

static void hold_ms(uint32_t milliseconds)
{
    while (milliseconds != 0u) {
        hold_us(1000u);
        milliseconds--;
    }
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void pixel_data_init_low(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    gpio.Pin = PIXEL_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIXEL_PORT, &gpio);
    PIXEL_PORT->BSRRH = PIXEL_PIN;
}

static void pixels_off(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    for (uint8_t bit = 0u; bit < PIXEL_OFF_BITS; bit++) {
        uint32_t start = DWT->CYCCNT;
        PIXEL_PORT->BSRRL = PIXEL_PIN;
        wait_cycles(start, PIXEL_T0H_CYCLES);
        PIXEL_PORT->BSRRH = PIXEL_PIN;
        wait_cycles(start, PIXEL_BIT_CYCLES);
    }
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    if (primask == 0u) {
        __enable_irq();
    }
    hold_us(PIXEL_RESET_US);
}

static void lamp_pwm_init(void)
{
    GPIO_InitTypeDef idle_gpio = {0};

    /* PA0 and PA1 drive the two independent 5 V tungsten channels.
     * This target tests only the 12 V tungsten channel on PA2, so force
     * both unused MOS PWM inputs low instead of leaving them floating. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    idle_gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    idle_gpio.Mode = GPIO_MODE_OUTPUT_PP;
    idle_gpio.Pull = GPIO_NOPULL;
    idle_gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &idle_gpio);

    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();

    gpio.Pin = LAMP_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF2_TIM5;
    HAL_GPIO_Init(LAMP_PORT, &gpio);

    TIM5->CR1 = 0u;
    TIM5->CR2 = 0u;
    TIM5->SMCR = 0u;
    TIM5->DIER = 0u;
    TIM5->PSC = 0u;
    TIM5->ARR = PWM_PERIOD_TICKS - 1u;
    TIM5->CCR3 = 0u;
    TIM5->CCMR2 = (TIM5->CCMR2 & ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S)) |
                  TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 |
                  TIM_CCMR2_OC3PE;
    TIM5->CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP);
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0u;
    TIM5->CCER |= TIM_CCER_CC3E;
    TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void lamp_set_duty(uint16_t duty)
{
    uint32_t period = TIM5->ARR + 1u;
    TIM5->CCR3 = (uint32_t)(((uint64_t)duty * period + 32767u) / 65535u);
}

static void wait_control_deadline(uint32_t *next_update)
{
    *next_update += CONTROL_PERIOD_CYCLES;
    while ((int32_t)(DWT->CYCCNT - *next_update) < 0) {
    }
}

static void ramp_duty(uint16_t from, uint16_t to, uint32_t duration_ms,
                      uint32_t *next_update)
{
    uint32_t frames = (CONTROL_RATE_HZ * duration_ms) / 1000u;
    int32_t delta = (int32_t)to - (int32_t)from;

    for (uint32_t frame = 0u; frame < frames; frame++) {
        int32_t value = (int32_t)from;
        if (frames > 1u) {
            value += (int32_t)(((int64_t)delta * frame) / (int64_t)(frames - 1u));
        }
        lamp_set_duty((uint16_t)value);
        wait_control_deadline(next_update);
    }
}

int main(void)
{
    uint32_t next_update;

    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u);
    dwt_init();
    pixel_data_init_low();
    pixels_off();
    lamp_pwm_init();

    lamp_set_duty(0u);
    hold_ms(STARTUP_OFF_MS);
    next_update = DWT->CYCCNT;

    ramp_duty(DUTY_START, DUTY_MAX, RAMP_UP_MS, &next_update);
    ramp_duty(DUTY_MAX, DUTY_START, RAMP_DOWN_MS, &next_update);
    ramp_duty(DUTY_START, 0u, FADE_OFF_MS, &next_update);
    lamp_set_duty(0u);

    /* Single-run safety state: remain off indefinitely for cooling. */
    while (1) {
        hold_ms(1000u);
    }
}
