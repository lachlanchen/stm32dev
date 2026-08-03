#include "main.h"
#include "sys.h"
#include <stdint.h>

/*
 * Independent H7 hardware-timer test for two daisy-chained SK6812RGBW pixels.
 * TIM2_CH4 drives PA3/A3 at an exact 800 kbit/s. The LCD/sensor app is not
 * linked into this image.
 */
#define PIXEL_PORT              GPIOA
#define PIXEL_PIN               GPIO_PIN_3
#define PIXEL_COUNT             2u
#define BITS_PER_PIXEL          32u
#define FRAME_BITS              (PIXEL_COUNT * BITS_PER_PIXEL)
#define PIXEL_LEVEL             64u
#define COLOR_HOLD_MS           3000u
#define CORE_CLOCK_HZ           400000000u

/* APB1=100 MHz and the x2 timer rule make TIM2 run at 200 MHz. */
#define TIM2_CLOCK_HZ           200000000u
#define DATA_RATE_HZ            800000u
#define TIMER_PERIOD_TICKS      (TIM2_CLOCK_HZ / DATA_RATE_HZ)
#define TIMER_T0H_TICKS         60u
#define TIMER_T1H_TICKS         120u
#define RESET_US                300u

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} Pixel;

static uint16_t frame_pulses[FRAME_BITS];

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    TIM2->CCR4 = 0u;
    TIM2->CCER &= ~TIM_CCER_CC4E;
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

static void tim2_ch4_pa3_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    gpio.Pin = PIXEL_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(PIXEL_PORT, &gpio);

    TIM2->CR1 = 0u;
    TIM2->CR2 = 0u;
    TIM2->SMCR = 0u;
    TIM2->DIER = 0u;
    TIM2->PSC = 0u;
    TIM2->ARR = TIMER_PERIOD_TICKS - 1u;
    TIM2->CCR4 = 0u;
    TIM2->CCMR2 = (TIM2->CCMR2 & ~(TIM_CCMR2_OC4M | TIM_CCMR2_CC4S)) |
                  TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 |
                  TIM_CCMR2_OC4PE;
    TIM2->CCER &= ~(TIM_CCER_CC4E | TIM_CCER_CC4P | TIM_CCER_CC4NP);
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
}

static void encode_byte(uint8_t value, uint16_t *index)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        frame_pulses[*index] = ((value & mask) != 0u) ?
                               TIMER_T1H_TICKS : TIMER_T0H_TICKS;
        (*index)++;
    }
}

static void encode_two_pixels(Pixel pixel)
{
    uint16_t index = 0u;

    for (uint8_t i = 0u; i < PIXEL_COUNT; i++) {
        /* Byte order verified on the working F103 rig: G, R, B, W. */
        encode_byte(pixel.g, &index);
        encode_byte(pixel.r, &index);
        encode_byte(pixel.b, &index);
        encode_byte(pixel.w, &index);
    }
}

static inline void wait_update(void)
{
    while ((TIM2->SR & TIM_SR_UIF) == 0u) {
    }
    TIM2->SR = 0u;
}

static void show_two(Pixel pixel)
{
    uint32_t primask;

    encode_two_pixels(pixel);
    primask = __get_PRIMASK();
    __disable_irq();

    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->CCER &= ~TIM_CCER_CC4E;
    TIM2->CNT = 0u;
    TIM2->CCR4 = frame_pulses[0];
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->CCER |= TIM_CCER_CC4E;
    TIM2->CR1 |= TIM_CR1_CEN;

    /* CCR4 preload makes every subsequent pulse start exactly at an update. */
    for (uint16_t i = 1u; i < FRAME_BITS; i++) {
        TIM2->CCR4 = frame_pulses[i];
        wait_update();
    }

    TIM2->CCR4 = 0u;
    wait_update();
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->CCER &= ~TIM_CCER_CC4E;

    if (primask == 0u) {
        __enable_irq();
    }
    hold_us(RESET_US);
}

int main(void)
{
    static const Pixel red = {PIXEL_LEVEL, 0u, 0u, 0u};
    static const Pixel green = {0u, PIXEL_LEVEL, 0u, 0u};
    static const Pixel blue = {0u, 0u, PIXEL_LEVEL, 0u};
    static const Pixel white = {0u, 0u, 0u, PIXEL_LEVEL};

    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u);
    dwt_init();
    tim2_ch4_pa3_init();

    show_two((Pixel){0u, 0u, 0u, 0u});
    hold_ms(500u);

    while (1) {
        show_two(red);
        hold_ms(COLOR_HOLD_MS);
        show_two(green);
        hold_ms(COLOR_HOLD_MS);
        show_two(blue);
        hold_ms(COLOR_HOLD_MS);
        show_two(white);
        hold_ms(COLOR_HOLD_MS);
    }
}
