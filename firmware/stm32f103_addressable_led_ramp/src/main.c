#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

#define FLASH_ACR       REG32(0x40022000u)
#define RCC_CR          REG32(0x40021000u)
#define RCC_CFGR        REG32(0x40021004u)
#define RCC_APB2ENR     REG32(0x40021018u)
#define GPIOA_CRL       REG32(0x40010800u)
#define GPIOA_BSRR      REG32(0x40010810u)
#define GPIOA_BRR       REG32(0x40010814u)
#define DEMCR           REG32(0xE000EDFCu)
#define DWT_CTRL        REG32(0xE0001000u)
#define DWT_CYCCNT      REG32(0xE0001004u)

#define PA0_MASK        (1u << 0)
#define CPU_HZ          64000000u
#define SK_BIT_CYCLES   80u
#define SK_T0H_CYCLES   22u
#define SK_T1H_CYCLES   45u
#define RESET_CYCLES    ((CPU_HZ / 1000000u) * 320u)
#define STEP_CYCLES     (CPU_HZ / 100u)

typedef enum {
    PIXEL_WS2812B_RGB = 0,
    PIXEL_SK6812_RGBW = 1
} pixel_kind_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t white;
} pixel_value_t;

/* Describe the real chain in DIN-to-DOUT order. */
static const pixel_kind_t PIXEL_KINDS[2] = {
    PIXEL_SK6812_RGBW,
    PIXEL_SK6812_RGBW
};

static void clock_init(void)
{
    FLASH_ACR = (1u << 4) | 2u;
    RCC_CFGR = (4u << 8) | (14u << 18);
    RCC_CR |= (1u << 24);
    while ((RCC_CR & (1u << 25)) == 0u) {
    }
    RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
    while ((RCC_CFGR & (3u << 2)) != (2u << 2)) {
    }
}

static void timing_init(void)
{
    DEMCR |= (1u << 24);
    DWT_CYCCNT = 0u;
    DWT_CTRL |= 1u;
}

static void delay_cycles(uint32_t cycles)
{
    const uint32_t start = DWT_CYCCNT;
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) {
    }
}

static void gpio_init(void)
{
    RCC_APB2ENR |= (1u << 2);
    GPIOA_CRL = (GPIOA_CRL & ~0xFu) | 0x3u;
    GPIOA_BRR = PA0_MASK;
}

static void send_bit(uint32_t one)
{
    const uint32_t start = DWT_CYCCNT;
    const uint32_t high_cycles = one ? SK_T1H_CYCLES : SK_T0H_CYCLES;

    GPIOA_BSRR = PA0_MASK;
    while ((uint32_t)(DWT_CYCCNT - start) < high_cycles) {
    }
    GPIOA_BRR = PA0_MASK;
    while ((uint32_t)(DWT_CYCCNT - start) < SK_BIT_CYCLES) {
    }
}

static void send_byte(uint8_t value)
{
    for (uint32_t mask = 0x80u; mask != 0u; mask >>= 1) {
        send_bit((value & mask) != 0u);
    }
}

static void send_pixel(pixel_kind_t kind, const pixel_value_t *value)
{
    /* Both verified batches use GRB first. SK6812 adds W as byte four. */
    send_byte(value->green);
    send_byte(value->red);
    send_byte(value->blue);
    if (kind == PIXEL_SK6812_RGBW) {
        send_byte(value->white);
    }
}

static void show(const pixel_value_t values[2])
{
    send_pixel(PIXEL_KINDS[0], &values[0]);
    send_pixel(PIXEL_KINDS[1], &values[1]);
    GPIOA_BRR = PA0_MASK;
    delay_cycles(RESET_CYCLES);
}

static pixel_value_t neutral_value(pixel_kind_t kind, uint8_t level)
{
    pixel_value_t value = {0u, 0u, 0u, 0u};

    if (kind == PIXEL_SK6812_RGBW) {
        value.white = level;
    } else {
        value.red = level;
        value.green = level;
        value.blue = level;
    }
    return value;
}

static void all_off(uint32_t milliseconds)
{
    const pixel_value_t values[2] = {
        {0u, 0u, 0u, 0u},
        {0u, 0u, 0u, 0u}
    };

    show(values);
    delay_cycles((CPU_HZ / 1000u) * milliseconds);
}

static void ramp_one(uint32_t pixel_index)
{
    pixel_value_t values[2] = {
        {0u, 0u, 0u, 0u},
        {0u, 0u, 0u, 0u}
    };

    for (uint32_t level = 0u; level <= 255u; ++level) {
        values[pixel_index] = neutral_value(PIXEL_KINDS[pixel_index],
                                            (uint8_t)level);
        show(values);
        delay_cycles(STEP_CYCLES);
    }

    for (int32_t level = 254; level >= 0; --level) {
        values[pixel_index] = neutral_value(PIXEL_KINDS[pixel_index],
                                            (uint8_t)level);
        show(values);
        delay_cycles(STEP_CYCLES);
    }
}

int main(void)
{
    clock_init();
    timing_init();
    gpio_init();

    while (1) {
        all_off(1000u);
        ramp_one(0u);
        all_off(500u);
        ramp_one(1u);
        all_off(2000u);
    }
}
