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
#define SK_T0H_CYCLES   19u
#define SK_T1H_CYCLES   39u
#define WHITE_LEVEL     48u

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

static void send_rgbw(uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
{
    send_byte(green);
    send_byte(red);
    send_byte(blue);
    send_byte(white);
}

static void send_static_frame(void)
{
    send_rgbw(WHITE_LEVEL, 0u, 0u, 0u);
    send_rgbw(0u, WHITE_LEVEL, 0u, 0u);
    GPIOA_BRR = PA0_MASK;
    delay_cycles(CPU_HZ / 10000u);
}

int main(void)
{
    clock_init();
    timing_init();
    gpio_init();

    while (1) {
        send_static_frame();
        delay_cycles(CPU_HZ / 20u);
    }
}
