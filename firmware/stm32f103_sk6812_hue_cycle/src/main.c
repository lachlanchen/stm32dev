#include <stdint.h>

/* STM32F103 medium-density device, verified ID 0x410 and 128 KiB flash. */
#define REG32(address)             (*(volatile uint32_t *)(address))

#define RCC_BASE                   0x40021000u
#define RCC_CR                     REG32(RCC_BASE + 0x00u)
#define RCC_CFGR                   REG32(RCC_BASE + 0x04u)
#define RCC_APB2ENR                REG32(RCC_BASE + 0x18u)

#define FLASH_ACR                  REG32(0x40022000u)

#define GPIOA_BASE                 0x40010800u
#define GPIOA_CRL                  REG32(GPIOA_BASE + 0x00u)
#define GPIOA_BSRR                 REG32(GPIOA_BASE + 0x10u)
#define GPIOA_BRR                  REG32(GPIOA_BASE + 0x14u)

#define COREDEBUG_DEMCR            REG32(0xE000EDFCu)
#define DWT_CTRL                   REG32(0xE0001000u)
#define DWT_CYCCNT                 REG32(0xE0001004u)

#define RCC_CR_HSION               (1u << 0)
#define RCC_CR_HSIRDY              (1u << 1)
#define RCC_CR_PLLON               (1u << 24)
#define RCC_CR_PLLRDY              (1u << 25)
#define RCC_CFGR_SW_MASK           (3u << 0)
#define RCC_CFGR_SW_PLL            (2u << 0)
#define RCC_CFGR_SWS_MASK          (3u << 2)
#define RCC_CFGR_SWS_PLL           (2u << 2)
#define RCC_CFGR_PPRE1_DIV2        (4u << 8)
#define RCC_CFGR_PLLMUL16          (14u << 18)
#define RCC_APB2ENR_IOPAEN         (1u << 2)
#define FLASH_ACR_LATENCY_2        (2u << 0)
#define FLASH_ACR_PRFTBE           (1u << 4)
#define COREDEBUG_TRCENA           (1u << 24)
#define DWT_CYCCNTENA              (1u << 0)

#define PA0                         (1u << 0)
#define CPU_HZ                      64000000u
#define SK6812_BIT_CYCLES           80u
#define SK6812_T0H_CYCLES           19u
#define SK6812_T1H_CYCLES           39u
#define SK6812_RESET_US             100u
#define DEMO_LEVEL                  96u
#define HUE_POSITIONS               1536u
#define HUE_STEP                    4u
#define FRAME_DELAY_MS              8u

/* Rev.01 data sheet order. Set to 1 if the physical part uses GRBW. */
#define SK6812_USE_GRBW_ORDER       1u

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} Pixel;

static void clock_init_64mhz(void)
{
    RCC_CR |= RCC_CR_HSION;
    while ((RCC_CR & RCC_CR_HSIRDY) == 0u) {}

    FLASH_ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;
    RCC_CFGR = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PLLMUL16;
    RCC_CR |= RCC_CR_PLLON;
    while ((RCC_CR & RCC_CR_PLLRDY) == 0u) {}

    RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {}
}

static void pa0_init(void)
{
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA_CRL = (GPIOA_CRL & ~0x0fu) | 0x03u; /* PA0 push-pull, 50 MHz. */
    GPIOA_BRR = PA0;
}

static void dwt_init(void)
{
    COREDEBUG_DEMCR |= COREDEBUG_TRCENA;
    DWT_CYCCNT = 0u;
    DWT_CTRL |= DWT_CYCCNTENA;
}

static inline void wait_cycles(uint32_t cycles)
{
    uint32_t start = DWT_CYCCNT;
    while ((uint32_t)(DWT_CYCCNT - start) < cycles) {}
}

static void delay_us(uint32_t us)
{
    while (us-- != 0u) {
        wait_cycles(CPU_HZ / 1000000u);
    }
}

static void delay_ms(uint32_t ms)
{
    while (ms-- != 0u) {
        delay_us(1000u);
    }
}

static inline void send_bit(uint8_t one)
{
    uint32_t high_cycles = one ? SK6812_T1H_CYCLES : SK6812_T0H_CYCLES;
    uint32_t start = DWT_CYCCNT;
    GPIOA_BSRR = PA0;
    while ((uint32_t)(DWT_CYCCNT - start) < high_cycles) {}
    GPIOA_BRR = PA0;
    while ((uint32_t)(DWT_CYCCNT - start) < SK6812_BIT_CYCLES) {}
}

static void send_byte(uint8_t value)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        send_bit((value & mask) != 0u);
    }
}

static void send_pixel(Pixel pixel)
{
#if SK6812_USE_GRBW_ORDER
    send_byte(pixel.g);
    send_byte(pixel.r);
#else
    send_byte(pixel.r);
    send_byte(pixel.g);
#endif
    send_byte(pixel.b);
    send_byte(pixel.w);
}

static void show_two(Pixel led1, Pixel led2)
{
    /* LED1 consumes the first 32 bits; LED2 consumes the next 32 bits. */
    send_pixel(led1);
    send_pixel(led2);
    GPIOA_BRR = PA0;
    delay_us(SK6812_RESET_US);
}

static uint8_t scale_channel(uint8_t value)
{
    return (uint8_t)(((uint16_t)value * DEMO_LEVEL + 127u) / 255u);
}

/*
 * Integer HSV hue wheel at full saturation and fixed value.
 * Six 256-position sectors give 1536 positions:
 * R -> Y -> G -> C -> B -> M -> R.
 */
static Pixel hue_to_rgbw(uint16_t hue)
{
    uint8_t sector = (uint8_t)(hue >> 8u);
    uint8_t offset = (uint8_t)(hue & 0xffu);
    uint8_t rising = offset;
    uint8_t falling = (uint8_t)(255u - offset);
    Pixel pixel = {0u, 0u, 0u, 0u};

    switch (sector) {
    case 0u:
        pixel.r = 255u;
        pixel.g = rising;
        break;
    case 1u:
        pixel.r = falling;
        pixel.g = 255u;
        break;
    case 2u:
        pixel.g = 255u;
        pixel.b = rising;
        break;
    case 3u:
        pixel.g = falling;
        pixel.b = 255u;
        break;
    case 4u:
        pixel.r = rising;
        pixel.b = 255u;
        break;
    default:
        pixel.r = 255u;
        pixel.b = falling;
        break;
    }

    pixel.r = scale_channel(pixel.r);
    pixel.g = scale_channel(pixel.g);
    pixel.b = scale_channel(pixel.b);
    return pixel;
}

int main(void)
{
    uint16_t hue = 0u;

    clock_init_64mhz();
    pa0_init();
    dwt_init();

    for (;;) {
        Pixel color = hue_to_rgbw(hue);
        show_two(color, color);
        delay_ms(FRAME_DELAY_MS);

        hue = (uint16_t)(hue + HUE_STEP);
        if (hue >= HUE_POSITIONS) {
            hue = (uint16_t)(hue - HUE_POSITIONS);
        }
    }
}
