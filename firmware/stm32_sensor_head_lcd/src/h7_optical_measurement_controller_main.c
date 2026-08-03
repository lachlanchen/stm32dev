#include "main.h"
#include "sys.h"
#include <stdint.h>
#include <string.h>

/*
 * Independent, host-armed optical measurement controller.
 *
 * PA2: TIM5_CH3, 443 Hz, 16-bit logical tungsten PWM.
 * PA3: TIM2_CH4, 800 kbit/s, one SK6812 RGBW pixel in GRBW wire order.
 *
 * The controller boots with every source off. A host writes one command into
 * optical_mailbox through SWD. Triangle sequences run from the H7's 1 kHz
 * scheduler and always terminate at zero. Direct mode has a watchdog.
 */

#define CORE_CLOCK_HZ                 400000000u
#define CONTROL_RATE_HZ               1000u
#define CONTROL_PERIOD_CYCLES         (CORE_CLOCK_HZ / CONTROL_RATE_HZ)

#define LAMP_PORT                     GPIOA
#define LAMP_PIN                      GPIO_PIN_2
#define TIM5_CLOCK_HZ                 200000000u
#define LAMP_PWM_HZ                   443u
#define LAMP_PERIOD_TICKS             ((TIM5_CLOCK_HZ + LAMP_PWM_HZ / 2u) / LAMP_PWM_HZ)

#define PIXEL_PORT                    GPIOA
#define PIXEL_PIN                     GPIO_PIN_3
#define PIXEL_BITS                    32u
#define PIXEL_DATA_RATE_HZ            800000u
#define TIM2_CLOCK_HZ                 200000000u
#define PIXEL_PERIOD_TICKS            (TIM2_CLOCK_HZ / PIXEL_DATA_RATE_HZ)
#define PIXEL_T0H_TICKS               60u
#define PIXEL_T1H_TICKS               120u
#define PIXEL_RESET_US                300u

#define MAILBOX_MAGIC                 0x4f505443u
#define MAILBOX_VERSION               1u
#define MODE_OFF                      0u
#define MODE_DIRECT                   1u
#define MODE_TUNGSTEN_TRIANGLE        2u
#define MODE_LED_CHANNEL_TRIANGLE     3u
#define MODE_LED_HUE_TRIANGLE         4u
#define STATUS_READY                  0u
#define STATUS_RUNNING                1u
#define STATUS_DIRECT                 2u
#define STATUS_WATCHDOG_OFF           3u
#define STATUS_CANCELLED              4u
#define DEFAULT_WATCHDOG_MS           750u
#define MAX_WATCHDOG_MS               2000u
#define MAX_HALF_PERIOD_US            10000000u
#define MAX_CYCLES                    100u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t command_seq;
    uint32_t ack_seq;
    uint32_t done_seq;
    uint32_t mode;
    uint32_t half_period_us;
    uint32_t cycles;
    uint32_t channel;
    uint32_t max_value;
    uint32_t pre_delay_ms;
    uint32_t direct_tungsten;
    uint32_t direct_rgbw;
    uint32_t watchdog_ms;
    uint32_t status;
    uint32_t applied_tungsten;
    uint32_t applied_rgbw;
    uint32_t sample_index;
    uint32_t total_samples;
    uint32_t heartbeat;
} OpticalMailbox;

volatile OpticalMailbox optical_mailbox __attribute__((used, aligned(32)));
static uint16_t pixel_pulses[PIXEL_BITS];

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void outputs_emergency_low(void)
{
    TIM5->CCR3 = 0u;
    TIM5->CCER &= ~TIM_CCER_CC3E;
    PIXEL_PORT->BSRRH = PIXEL_PIN;
}

void Error_Handler(void)
{
    outputs_emergency_low();
    __disable_irq();
    while (1) {
    }
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
    while (milliseconds-- != 0u) {
        hold_us(1000u);
    }
}

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void unused_lamps_low(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void lamp_pwm_init(void)
{
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
    TIM5->ARR = LAMP_PERIOD_TICKS - 1u;
    TIM5->CCR3 = 0u;
    TIM5->CCMR2 = (TIM5->CCMR2 & ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S)) |
                  TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
    TIM5->CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP);
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0u;
    TIM5->CCER |= TIM_CCER_CC3E;
    TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void lamp_set(uint32_t duty)
{
    uint32_t bounded = duty > 65535u ? 65535u : duty;
    uint32_t period = TIM5->ARR + 1u;
    TIM5->CCR3 = (uint32_t)(((uint64_t)bounded * period + 32767u) / 65535u);
    optical_mailbox.applied_tungsten = bounded;
}

static void pa3_force_low(void)
{
    GPIO_InitTypeDef gpio = {0};

    PIXEL_PORT->BSRRH = PIXEL_PIN;
    gpio.Pin = PIXEL_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIXEL_PORT, &gpio);
    PIXEL_PORT->BSRRH = PIXEL_PIN;
}

static void pa3_timer_mode(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = PIXEL_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(PIXEL_PORT, &gpio);
}

static void pixel_timer_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    pa3_force_low();
    TIM2->CR1 = 0u;
    TIM2->CR2 = 0u;
    TIM2->SMCR = 0u;
    TIM2->DIER = 0u;
    TIM2->PSC = 0u;
    TIM2->ARR = PIXEL_PERIOD_TICKS - 1u;
    TIM2->CCR4 = 0u;
    TIM2->CCMR2 = (TIM2->CCMR2 & ~(TIM_CCMR2_OC4M | TIM_CCMR2_CC4S)) |
                  TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4PE;
    TIM2->CCER &= ~(TIM_CCER_CC4E | TIM_CCER_CC4P | TIM_CCER_CC4NP);
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
}

static void encode_byte(uint8_t value, uint16_t *index)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        pixel_pulses[*index] = (value & mask) != 0u ?
                               PIXEL_T1H_TICKS : PIXEL_T0H_TICKS;
        (*index)++;
    }
}

static inline void wait_timer2_update(void)
{
    while ((TIM2->SR & TIM_SR_UIF) == 0u) {
    }
    TIM2->SR = 0u;
}

static void pixel_show(uint32_t rgbw)
{
    uint8_t r = (uint8_t)(rgbw & 0xffu);
    uint8_t g = (uint8_t)((rgbw >> 8u) & 0xffu);
    uint8_t b = (uint8_t)((rgbw >> 16u) & 0xffu);
    uint8_t w = (uint8_t)((rgbw >> 24u) & 0xffu);
    uint16_t index = 0u;
    uint32_t primask;

    /* SK6812 wire order is G, R, B, W. */
    encode_byte(g, &index);
    encode_byte(r, &index);
    encode_byte(b, &index);
    encode_byte(w, &index);

    primask = __get_PRIMASK();
    __disable_irq();
    pa3_timer_mode();
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->CCER &= ~TIM_CCER_CC4E;
    TIM2->CNT = 0u;
    TIM2->CCR4 = pixel_pulses[0];
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->CCER |= TIM_CCER_CC4E;
    TIM2->CR1 |= TIM_CR1_CEN;
    for (uint16_t i = 1u; i < PIXEL_BITS; i++) {
        TIM2->CCR4 = pixel_pulses[i];
        wait_timer2_update();
    }
    TIM2->CCR4 = 0u;
    wait_timer2_update();
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM2->CCER &= ~TIM_CCER_CC4E;
    pa3_force_low();
    if (primask == 0u) {
        __enable_irq();
    }
    hold_us(PIXEL_RESET_US);
    optical_mailbox.applied_rgbw = rgbw;
}

static void all_off(uint32_t *last_rgbw)
{
    lamp_set(0u);
    if (*last_rgbw != 0u) {
        pixel_show(0u);
        *last_rgbw = 0u;
    }
    optical_mailbox.applied_tungsten = 0u;
    optical_mailbox.applied_rgbw = 0u;
}

static uint32_t pack_channel(uint32_t channel, uint32_t value)
{
    uint32_t bounded = value > 255u ? 255u : value;
    if (channel > 3u) {
        return 0u;
    }
    return bounded << (channel * 8u);
}

static uint32_t hue_to_rgbw(uint32_t hue, uint32_t brightness)
{
    uint32_t bounded_hue = hue > 1024u ? 1024u : hue;
    uint32_t bounded_level = brightness > 255u ? 255u : brightness;
    uint32_t sector = bounded_hue >> 8u;
    uint32_t offset = bounded_hue & 0xffu;
    uint32_t rising = (offset * bounded_level + 127u) / 255u;
    uint32_t falling = bounded_level - rising;
    uint32_t r = 0u;
    uint32_t g = 0u;
    uint32_t b = 0u;

    switch (sector) {
    case 0u:
        r = bounded_level;
        g = rising;
        break;
    case 1u:
        r = falling;
        g = bounded_level;
        break;
    case 2u:
        g = bounded_level;
        b = rising;
        break;
    case 3u:
        g = falling;
        b = bounded_level;
        break;
    default:
        b = bounded_level;
        break;
    }
    return r | (g << 8u) | (b << 16u);
}

static uint32_t triangle_level(uint32_t frame, uint32_t half_frames,
                               uint32_t max_value)
{
    uint32_t cycle_frames = half_frames * 2u;
    uint32_t position = frame % cycle_frames;
    uint32_t numerator;

    if (half_frames <= 1u) {
        return max_value;
    }
    if (position < half_frames) {
        numerator = position;
    } else {
        numerator = cycle_frames - 1u - position;
    }
    return (uint32_t)(((uint64_t)max_value * numerator) / (half_frames - 1u));
}

static uint8_t run_triangle(uint32_t sequence, uint32_t mode,
                            uint32_t half_period_us, uint32_t cycles,
                            uint32_t channel, uint32_t max_value,
                            uint32_t pre_delay_ms, uint32_t *last_rgbw)
{
    uint32_t half_frames = (half_period_us + 500u) / 1000u;
    uint32_t total_frames;
    uint32_t next_frame;

    if (half_frames == 0u) {
        half_frames = 1u;
    }
    total_frames = half_frames * 2u * cycles;
    optical_mailbox.total_samples = total_frames;
    optical_mailbox.sample_index = 0u;
    hold_ms(pre_delay_ms);
    next_frame = DWT->CYCCNT;

    for (uint32_t frame = 0u; frame < total_frames; frame++) {
        uint32_t level;
        uint32_t rgbw;

        if (optical_mailbox.command_seq != sequence) {
            return 0u;
        }
        level = triangle_level(frame, half_frames,
                               mode == MODE_LED_HUE_TRIANGLE ? 1024u : max_value);
        if (mode == MODE_TUNGSTEN_TRIANGLE) {
            lamp_set(level);
        } else if (mode == MODE_LED_HUE_TRIANGLE) {
            rgbw = hue_to_rgbw(level, max_value);
            if (rgbw != *last_rgbw) {
                pixel_show(rgbw);
                *last_rgbw = rgbw;
            }
        } else {
            rgbw = pack_channel(channel, level);
            if (rgbw != *last_rgbw) {
                pixel_show(rgbw);
                *last_rgbw = rgbw;
            }
        }
        optical_mailbox.sample_index = frame + 1u;
        next_frame += CONTROL_PERIOD_CYCLES;
        while ((int32_t)(DWT->CYCCNT - next_frame) < 0) {
        }
    }
    return 1u;
}

int main(void)
{
    uint32_t handled_sequence = 0u;
    uint32_t last_rgbw = 0xffffffffu;
    uint32_t direct_command_cycle = 0u;
    uint32_t direct_watchdog_ms = DEFAULT_WATCHDOG_MS;

    /* SWD writes must be immediately visible to this polling firmware. */
    SCB_EnableICache();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u);
    dwt_init();
    unused_lamps_low();
    lamp_pwm_init();
    pixel_timer_init();
    memset((void *)&optical_mailbox, 0, sizeof(optical_mailbox));
    optical_mailbox.magic = MAILBOX_MAGIC;
    optical_mailbox.version = MAILBOX_VERSION;
    optical_mailbox.status = STATUS_READY;
    pixel_show(0u);
    last_rgbw = 0u;
    lamp_set(0u);

    while (1) {
        uint32_t sequence = optical_mailbox.command_seq;

        if (sequence != handled_sequence) {
            uint32_t mode;
            uint32_t half_period_us;
            uint32_t cycles;
            uint32_t channel;
            uint32_t max_value;
            uint32_t pre_delay_ms;
            uint32_t watchdog_ms;
            uint8_t completed = 1u;

            __DMB();
            mode = optical_mailbox.mode;
            half_period_us = optical_mailbox.half_period_us;
            cycles = optical_mailbox.cycles;
            channel = optical_mailbox.channel;
            max_value = optical_mailbox.max_value;
            pre_delay_ms = optical_mailbox.pre_delay_ms;
            watchdog_ms = optical_mailbox.watchdog_ms;
            handled_sequence = sequence;
            optical_mailbox.ack_seq = sequence;
            optical_mailbox.done_seq = 0u;
            optical_mailbox.status = STATUS_RUNNING;

            if (half_period_us > MAX_HALF_PERIOD_US) {
                half_period_us = MAX_HALF_PERIOD_US;
            }
            if (cycles == 0u || cycles > MAX_CYCLES) {
                cycles = 1u;
            }
            if (pre_delay_ms > 2000u) {
                pre_delay_ms = 2000u;
            }

            if (mode == MODE_DIRECT) {
                uint32_t rgbw = optical_mailbox.direct_rgbw;
                lamp_set(optical_mailbox.direct_tungsten);
                if (rgbw != last_rgbw) {
                    pixel_show(rgbw);
                    last_rgbw = rgbw;
                }
                direct_watchdog_ms = watchdog_ms == 0u ?
                                     DEFAULT_WATCHDOG_MS : watchdog_ms;
                if (direct_watchdog_ms > MAX_WATCHDOG_MS) {
                    direct_watchdog_ms = MAX_WATCHDOG_MS;
                }
                direct_command_cycle = DWT->CYCCNT;
                optical_mailbox.status = STATUS_DIRECT;
                optical_mailbox.done_seq = sequence;
            } else if (mode == MODE_TUNGSTEN_TRIANGLE ||
                       mode == MODE_LED_CHANNEL_TRIANGLE ||
                       mode == MODE_LED_HUE_TRIANGLE) {
                all_off(&last_rgbw);
                completed = run_triangle(sequence, mode, half_period_us,
                                         cycles, channel, max_value,
                                         pre_delay_ms, &last_rgbw);
                all_off(&last_rgbw);
                optical_mailbox.status = completed != 0u ?
                                         STATUS_READY : STATUS_CANCELLED;
                optical_mailbox.done_seq = sequence;
            } else {
                all_off(&last_rgbw);
                optical_mailbox.status = STATUS_READY;
                optical_mailbox.done_seq = sequence;
            }
            __DMB();
        }

        if (optical_mailbox.status == STATUS_DIRECT) {
            uint32_t limit = (CORE_CLOCK_HZ / 1000u) * direct_watchdog_ms;
            if ((uint32_t)(DWT->CYCCNT - direct_command_cycle) > limit) {
                all_off(&last_rgbw);
                optical_mailbox.status = STATUS_WATCHDOG_OFF;
            }
        }
        optical_mailbox.heartbeat++;
        hold_us(100u);
    }
}
