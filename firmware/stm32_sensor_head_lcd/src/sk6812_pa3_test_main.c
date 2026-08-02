#include "main.h"
#include "sys.h"
#include "delay.h"
#include <stdint.h>

/*
 * Independent one-pixel SK6812RGBW test for the STM32H743 board.
 * Data output: PA3 / header A3. The LCD/sensor application in main.c is not
 * linked into this image and can be restored independently.
 */
#define PIXEL_PORT          GPIOA
#define PIXEL_PIN           GPIO_PIN_3
#define PIXEL_DATA_HZ       800000u
#define PIXEL_T0H_NS        300u
#define PIXEL_T1H_NS        600u
#define PIXEL_RESET_US      300u
#define PIXEL_LEVEL         64u
#define COLOR_HOLD_MS       3000u

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
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    while (1) {}
}

static void pixel_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin = PIXEL_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIXEL_PORT, &gpio);
    PIXEL_PORT->BSRRH = PIXEL_PIN;
}

static void pixel_timing_init(void)
{
    uint32_t core_hz;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* DWT CYCCNT follows the 400 MHz Cortex-M7 core clock, not HCLK. */
    SystemCoreClockUpdate();
    core_hz = SystemCoreClock;
    bit_cycles = core_hz / PIXEL_DATA_HZ;
    t0h_cycles = (uint32_t)(((uint64_t)core_hz * PIXEL_T0H_NS) / 1000000000ull);
    t1h_cycles = (uint32_t)(((uint64_t)core_hz * PIXEL_T1H_NS) / 1000000000ull);
}

static inline void wait_cycles(uint32_t start, uint32_t count)
{
    while ((uint32_t)(DWT->CYCCNT - start) < count) {
        __NOP();
    }
}

static inline void pixel_write_bit(uint8_t one)
{
    uint32_t high_cycles = one ? t1h_cycles : t0h_cycles;
    uint32_t start = DWT->CYCCNT;

    PIXEL_PORT->BSRRL = PIXEL_PIN;
    wait_cycles(start, high_cycles);
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    wait_cycles(start, bit_cycles);
}

static void pixel_write_byte(uint8_t value)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        pixel_write_bit((value & mask) != 0u);
    }
}

static void pixel_show(uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    /* Verified SK6812RGBW byte order for this pixel batch: G, R, B, W. */
    pixel_write_byte(green);
    pixel_write_byte(red);
    pixel_write_byte(blue);
    pixel_write_byte(white);
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    if (primask == 0u) {
        __enable_irq();
    }
    delay_us(PIXEL_RESET_US);
}

int main(void)
{
    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u); /* 400 MHz Cortex-M7 core. */
    delay_init(400u);
    pixel_gpio_init();
    pixel_timing_init();

    pixel_show(0u, 0u, 0u, 0u);
    HAL_Delay(500u);

    while (1) {
        pixel_show(PIXEL_LEVEL, 0u, 0u, 0u);
        HAL_Delay(COLOR_HOLD_MS);
        pixel_show(0u, PIXEL_LEVEL, 0u, 0u);
        HAL_Delay(COLOR_HOLD_MS);
        pixel_show(0u, 0u, PIXEL_LEVEL, 0u);
        HAL_Delay(COLOR_HOLD_MS);
        pixel_show(0u, 0u, 0u, PIXEL_LEVEL);
        HAL_Delay(COLOR_HOLD_MS);
    }
}
