#include "main.h"
#include "sys.h"
#include "delay.h"
#include <stdint.h>

/*
 * Independent two-pixel SK6812RGBW runner for the second STM32H743 board.
 * Data output: PA0 / header A0.
 * The normal TSL2591 + AS7343 application remains in src/main.c.
 */
#define SK6812_PORT             GPIOA
#define SK6812_PIN              GPIO_PIN_0
#define SK6812_PIXEL_COUNT      2u
#define SK6812_RESET_US         100u
#define SK6812_DATA_HZ          800000u
#define SK6812_T0H_NS           300u
#define SK6812_T1H_NS           600u

/* Safe bring-up level. Increase only after power, decoupling, and color order work. */
#define RUNNER_LEVEL            48u
#define RUNNER_STEPS            64u
#define RUNNER_FRAME_MS         12u

/* The cited SK6812RGBW Rev.01 data sheet specifies R,G,B,W byte order. */
#define SK6812_USE_GRBW_ORDER   0u

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} Sk6812Pixel;

static uint32_t bit_cycles;
static uint32_t t0h_cycles;
static uint32_t t1h_cycles;

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    SK6812_PORT->BSRRH = SK6812_PIN;
    while (1) {}
}

static void sk6812_gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = SK6812_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SK6812_PORT, &gpio);
    SK6812_PORT->BSRRH = SK6812_PIN;
}

static void sk6812_timing_init(void)
{
    uint32_t hclk = HAL_RCC_GetHCLKFreq();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    bit_cycles = hclk / SK6812_DATA_HZ;
    t0h_cycles = (uint32_t)(((uint64_t)hclk * SK6812_T0H_NS) / 1000000000ull);
    t1h_cycles = (uint32_t)(((uint64_t)hclk * SK6812_T1H_NS) / 1000000000ull);
}

static inline void wait_cycles(uint32_t start, uint32_t count)
{
    while ((uint32_t)(DWT->CYCCNT - start) < count) {
        __NOP();
    }
}

static inline void sk6812_write_bit(uint8_t one)
{
    uint32_t high_cycles = one ? t1h_cycles : t0h_cycles;

    SK6812_PORT->BSRRL = SK6812_PIN;
    uint32_t start = DWT->CYCCNT;
    wait_cycles(start, high_cycles);
    SK6812_PORT->BSRRH = SK6812_PIN;
    wait_cycles(start, bit_cycles);
}

static void sk6812_write_byte(uint8_t value)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        sk6812_write_bit((value & mask) != 0u);
    }
}

static void sk6812_write_pixel(const Sk6812Pixel *pixel)
{
#if SK6812_USE_GRBW_ORDER
    sk6812_write_byte(pixel->g);
    sk6812_write_byte(pixel->r);
#else
    sk6812_write_byte(pixel->r);
    sk6812_write_byte(pixel->g);
#endif
    sk6812_write_byte(pixel->b);
    sk6812_write_byte(pixel->w);
}

static void sk6812_show(const Sk6812Pixel pixels[SK6812_PIXEL_COUNT])
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    for (uint8_t i = 0u; i < SK6812_PIXEL_COUNT; i++) {
        sk6812_write_pixel(&pixels[i]);
    }

    SK6812_PORT->BSRRH = SK6812_PIN;
    if (primask == 0u) {
        __enable_irq();
    }
    delay_us(SK6812_RESET_US);
}

static uint8_t lerp_u8(uint8_t a, uint8_t b, uint16_t step)
{
    int32_t delta = (int32_t)b - (int32_t)a;
    return (uint8_t)((int32_t)a + (delta * (int32_t)step) / (int32_t)RUNNER_STEPS);
}

static Sk6812Pixel lerp_pixel(Sk6812Pixel a, Sk6812Pixel b, uint16_t step)
{
    Sk6812Pixel out;
    out.r = lerp_u8(a.r, b.r, step);
    out.g = lerp_u8(a.g, b.g, step);
    out.b = lerp_u8(a.b, b.b, step);
    out.w = lerp_u8(a.w, b.w, step);
    return out;
}

static void transition(Sk6812Pixel from0, Sk6812Pixel from1,
                       Sk6812Pixel to0, Sk6812Pixel to1)
{
    Sk6812Pixel pixels[SK6812_PIXEL_COUNT];

    for (uint16_t step = 0u; step <= RUNNER_STEPS; step++) {
        pixels[0] = lerp_pixel(from0, to0, step);
        pixels[1] = lerp_pixel(from1, to1, step);
        sk6812_show(pixels);
        HAL_Delay(RUNNER_FRAME_MS);
    }
}

int main(void)
{
    static const Sk6812Pixel off = {0u, 0u, 0u, 0u};
    static const Sk6812Pixel palette[] = {
        {RUNNER_LEVEL, 0u, 0u, 0u},
        {0u, RUNNER_LEVEL, 0u, 0u},
        {0u, 0u, RUNNER_LEVEL, 0u},
        {0u, 0u, 0u, RUNNER_LEVEL}
    };
    Sk6812Pixel pixels[SK6812_PIXEL_COUNT] = {off, off};

    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u); /* 400 MHz HCLK */
    delay_init(400u);
    sk6812_gpio_init();
    sk6812_timing_init();

    sk6812_show(pixels);
    HAL_Delay(500u);

    while (1) {
        for (uint8_t color = 0u; color < 4u; color++) {
            uint8_t next = (uint8_t)((color + 1u) & 3u);
            transition(palette[color], off, off, palette[color]);
            transition(off, palette[color], palette[next], off);
        }
    }
}
