#include "main.h"
#include "sys.h"
#include <stdint.h>
#include <string.h>

/*
 * Independent, host-armed optical joint-scan controller with TSL2591 logging.
 *
 * PA2: TIM5_CH3, 443 Hz, 16-bit logical tungsten PWM.
 * PA3: TIM2_CH4, 800 kbit/s, one SK6812 RGBW pixel in GRBW wire order.
 *
 * The controller boots with every source off. A host writes one command into
 * optical_mailbox through SWD. Triangle sequences run from the H7's 1 kHz
 * scheduler and always terminate at zero. Direct mode has a watchdog. A host
 * may also upload a finite table of synchronized tungsten/RGBW states into
 * optical_schedule. TSL2591 reads are timestamped in RAM and retrieved through
 * SWD after the finite scan, so host plotting never perturbs acquisition.
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
#define MAILBOX_VERSION               2u
#define MODE_OFF                      0u
#define MODE_DIRECT                   1u
#define MODE_TUNGSTEN_TRIANGLE        2u
#define MODE_LED_CHANNEL_TRIANGLE     3u
#define MODE_LED_HUE_TRIANGLE         4u
#define MODE_TABLE_SCAN               5u
#define STATUS_READY                  0u
#define STATUS_RUNNING                1u
#define STATUS_DIRECT                 2u
#define STATUS_WATCHDOG_OFF           3u
#define STATUS_CANCELLED              4u
#define DEFAULT_WATCHDOG_MS           750u
#define MAX_WATCHDOG_MS               2000u
#define MAX_HALF_PERIOD_US            10000000u
#define MAX_CYCLES                    100u
#define OPTICAL_SCHEDULE_CAPACITY     4096u
#define MIN_TABLE_DWELL_US            500u
#define MAX_TABLE_DWELL_US            1000000u
#define MAX_TABLE_REPEATS             100u
#define TSL2591_ADDR                  0x29u
#define TSL2591_CMD                   0xa0u
#define TSL_SCL_PIN                   GPIO_PIN_8
#define TSL_SDA_PIN                   GPIO_PIN_9
#define TSL_LOG_MAGIC                 0x54534c47u
#define TSL_LOG_CAPACITY              512u
#define TSL_LOG_PERIOD_US             20000u
#define TSL_EXPECTED_ID               0x50u

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
    uint32_t table_count;
    uint32_t table_repeats;
    uint32_t table_dwell_us;
} OpticalMailbox;

typedef struct {
    uint32_t tungsten;
    uint32_t rgbw;
} OpticalState;

typedef struct {
    uint32_t table_sample;
    uint32_t timestamp_us;
    uint16_t ch0_full;
    uint16_t ch1_ir;
    uint16_t visible;
    uint8_t valid;
    uint8_t gain_code;
} TslLogEntry;

volatile OpticalMailbox optical_mailbox __attribute__((used, aligned(32)));
volatile OpticalState optical_schedule[OPTICAL_SCHEDULE_CAPACITY]
    __attribute__((used, aligned(32)));
volatile uint32_t tsl_log_magic __attribute__((used)) = TSL_LOG_MAGIC;
volatile uint32_t tsl_log_capacity __attribute__((used)) = TSL_LOG_CAPACITY;
volatile uint32_t tsl_log_record_size __attribute__((used)) = sizeof(TslLogEntry);
volatile uint32_t tsl_log_count __attribute__((used));
volatile uint32_t tsl_log_dropped __attribute__((used));
volatile uint32_t tsl_sensor_status __attribute__((used));
volatile uint32_t tsl_sensor_id __attribute__((used));
volatile uint32_t tsl_gain_code __attribute__((used)) = 0x10u;
volatile TslLogEntry tsl_log[TSL_LOG_CAPACITY]
    __attribute__((used, aligned(32)));
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

static inline void tsl_bus_delay(void)
{
    uint32_t start = DWT->CYCCNT;
    wait_cycles(start, CORE_CLOCK_HZ / 500000u);
}

static inline void tsl_scl_release(void)
{
    GPIOB->BSRRL = TSL_SCL_PIN;
}

static inline void tsl_scl_low(void)
{
    GPIOB->BSRRH = TSL_SCL_PIN;
}

static inline void tsl_sda_release(void)
{
    GPIOB->BSRRL = TSL_SDA_PIN;
}

static inline void tsl_sda_low(void)
{
    GPIOB->BSRRH = TSL_SDA_PIN;
}

static inline uint8_t tsl_sda_read(void)
{
    return (GPIOB->IDR & TSL_SDA_PIN) != 0u ? 1u : 0u;
}

static void tsl_bus_recover(void)
{
    tsl_sda_release();
    for (uint32_t i = 0u; i < 9u && tsl_sda_read() == 0u; i++) {
        tsl_scl_low();
        tsl_bus_delay();
        tsl_scl_release();
        tsl_bus_delay();
    }
    tsl_sda_low();
    tsl_bus_delay();
    tsl_scl_release();
    tsl_bus_delay();
    tsl_sda_release();
    tsl_bus_delay();
}

static void tsl_bus_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = TSL_SCL_PIN | TSL_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    tsl_scl_release();
    tsl_sda_release();
    tsl_bus_recover();
    tsl_sensor_status = 1u;
}

static void tsl_start(void)
{
    tsl_sda_release();
    tsl_scl_release();
    tsl_bus_delay();
    tsl_sda_low();
    tsl_bus_delay();
    tsl_scl_low();
}

static void tsl_stop(void)
{
    tsl_sda_low();
    tsl_bus_delay();
    tsl_scl_release();
    tsl_bus_delay();
    tsl_sda_release();
    tsl_bus_delay();
}

static uint8_t tsl_write_byte(uint8_t value)
{
    for (uint8_t mask = 0x80u; mask != 0u; mask >>= 1u) {
        if ((value & mask) != 0u) {
            tsl_sda_release();
        } else {
            tsl_sda_low();
        }
        tsl_bus_delay();
        tsl_scl_release();
        tsl_bus_delay();
        tsl_scl_low();
    }
    tsl_sda_release();
    tsl_bus_delay();
    tsl_scl_release();
    tsl_bus_delay();
    uint8_t acknowledged = tsl_sda_read() == 0u ? 1u : 0u;
    tsl_scl_low();
    return acknowledged;
}

static uint8_t tsl_read_byte(uint8_t acknowledge)
{
    uint8_t value = 0u;

    tsl_sda_release();
    for (uint8_t bit = 0u; bit < 8u; bit++) {
        value <<= 1u;
        tsl_bus_delay();
        tsl_scl_release();
        tsl_bus_delay();
        value |= tsl_sda_read();
        tsl_scl_low();
    }
    if (acknowledge != 0u) {
        tsl_sda_low();
    } else {
        tsl_sda_release();
    }
    tsl_bus_delay();
    tsl_scl_release();
    tsl_bus_delay();
    tsl_scl_low();
    tsl_sda_release();
    return value;
}

static uint8_t tsl_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t ok;

    tsl_start();
    ok = tsl_write_byte((uint8_t)(TSL2591_ADDR << 1u));
    ok = tsl_write_byte((uint8_t)(TSL2591_CMD | reg)) && ok;
    ok = tsl_write_byte(value) && ok;
    tsl_stop();
    return ok;
}

static uint8_t tsl_read_bytes(uint8_t reg, uint8_t *data, uint32_t count)
{
    uint8_t ok;

    tsl_start();
    ok = tsl_write_byte((uint8_t)(TSL2591_ADDR << 1u));
    ok = tsl_write_byte((uint8_t)(TSL2591_CMD | reg)) && ok;
    tsl_start();
    ok = tsl_write_byte((uint8_t)((TSL2591_ADDR << 1u) | 1u)) && ok;
    if (ok != 0u) {
        for (uint32_t i = 0u; i < count; i++) {
            data[i] = tsl_read_byte(i + 1u < count ? 1u : 0u);
        }
    }
    tsl_stop();
    return ok;
}

static uint8_t tsl_read16(uint8_t reg, uint16_t *value)
{
    uint8_t raw[2];
    if (tsl_read_bytes(reg, raw, sizeof(raw)) == 0u) {
        return 0u;
    }
    *value = (uint16_t)(((uint16_t)raw[1] << 8u) | raw[0]);
    return 1u;
}

static uint8_t tsl2591_configure(uint8_t gain_code)
{
    uint8_t id = 0u;
    uint8_t ok;

    tsl_sensor_status = 1u;
    tsl_bus_recover();
    ok = tsl_read_bytes(0x12u, &id, 1u);
    tsl_sensor_id = id;
    if (ok != 0u) {
        tsl_sensor_status |= 2u;
    }
    if (id == TSL_EXPECTED_ID) {
        tsl_sensor_status |= 4u;
    }
    gain_code &= 0x30u;
    ok = tsl_write_reg(0x00u, 0x01u) && ok;
    ok = tsl_write_reg(0x01u, gain_code) && ok;
    ok = tsl_write_reg(0x00u, 0x03u) && ok;
    if (ok != 0u) {
        tsl_sensor_status |= 8u;
    }
    hold_ms(120u);
    return ok;
}

static void tsl_log_reset(void)
{
    tsl_log_magic = TSL_LOG_MAGIC;
    tsl_log_capacity = TSL_LOG_CAPACITY;
    tsl_log_record_size = sizeof(TslLogEntry);
    tsl_log_count = 0u;
    tsl_log_dropped = 0u;
    memset((void *)tsl_log, 0, sizeof(tsl_log));
    __DMB();
}

static void tsl_log_sample(uint32_t table_sample, uint32_t scan_start_cycle)
{
    uint32_t index = tsl_log_count;
    uint16_t ch0 = 0u;
    uint16_t ch1 = 0u;
    uint8_t valid = tsl_read16(0x14u, &ch0) && tsl_read16(0x16u, &ch1);

    if (index >= TSL_LOG_CAPACITY) {
        tsl_log_dropped++;
        return;
    }
    tsl_log[index].table_sample = table_sample;
    tsl_log[index].timestamp_us = (uint32_t)(DWT->CYCCNT - scan_start_cycle) /
                                  (CORE_CLOCK_HZ / 1000000u);
    tsl_log[index].ch0_full = ch0;
    tsl_log[index].ch1_ir = ch1;
    tsl_log[index].visible = ch0 > ch1 ? (uint16_t)(ch0 - ch1) : 0u;
    tsl_log[index].valid = valid;
    tsl_log[index].gain_code = (uint8_t)(tsl_gain_code & 0x30u);
    if (valid != 0u) {
        tsl_sensor_status |= 16u;
    }
    __DMB();
    tsl_log_count = index + 1u;
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

static uint8_t run_table(uint32_t sequence, uint32_t table_count,
                         uint32_t table_repeats, uint32_t table_dwell_us,
                         uint32_t pre_delay_ms, uint32_t *last_rgbw)
{
    uint32_t next_frame;
    uint32_t dwell_cycles;
    uint32_t sample = 0u;
    uint32_t scan_start_cycle;
    uint32_t next_tsl_cycle;
    const uint32_t tsl_period_cycles = (CORE_CLOCK_HZ / 1000000u) *
                                       TSL_LOG_PERIOD_US;

    if (table_count == 0u || table_count > OPTICAL_SCHEDULE_CAPACITY) {
        return 0u;
    }
    if (table_repeats == 0u || table_repeats > MAX_TABLE_REPEATS) {
        table_repeats = 1u;
    }
    if (table_dwell_us < MIN_TABLE_DWELL_US) {
        table_dwell_us = MIN_TABLE_DWELL_US;
    }
    if (table_dwell_us > MAX_TABLE_DWELL_US) {
        table_dwell_us = MAX_TABLE_DWELL_US;
    }
    dwell_cycles = (CORE_CLOCK_HZ / 1000000u) * table_dwell_us;
    tsl_log_reset();
    (void)tsl2591_configure((uint8_t)tsl_gain_code);
    optical_mailbox.total_samples = table_count * table_repeats;
    optical_mailbox.sample_index = 0u;
    hold_ms(pre_delay_ms);
    scan_start_cycle = DWT->CYCCNT;
    next_frame = scan_start_cycle;
    next_tsl_cycle = scan_start_cycle + tsl_period_cycles;

    for (uint32_t repeat = 0u; repeat < table_repeats; repeat++) {
        for (uint32_t index = 0u; index < table_count; index++) {
            uint32_t tungsten;
            uint32_t rgbw;

            if (optical_mailbox.command_seq != sequence) {
                return 0u;
            }
            __DMB();
            tungsten = optical_schedule[index].tungsten;
            rgbw = optical_schedule[index].rgbw;
            lamp_set(tungsten);
            if (rgbw != *last_rgbw) {
                pixel_show(rgbw);
                *last_rgbw = rgbw;
            }
            sample++;
            optical_mailbox.sample_index = sample;
            if ((int32_t)(DWT->CYCCNT - next_tsl_cycle) >= 0) {
                tsl_log_sample(sample, scan_start_cycle);
                do {
                    next_tsl_cycle += tsl_period_cycles;
                } while ((int32_t)(DWT->CYCCNT - next_tsl_cycle) >= 0);
            }
            next_frame += dwell_cycles;
            while ((int32_t)(DWT->CYCCNT - next_frame) < 0) {
            }
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
    tsl_bus_init();
    tsl_log_reset();
    (void)tsl2591_configure((uint8_t)tsl_gain_code);
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
            uint32_t table_count;
            uint32_t table_repeats;
            uint32_t table_dwell_us;
            uint8_t completed = 1u;

            __DMB();
            mode = optical_mailbox.mode;
            half_period_us = optical_mailbox.half_period_us;
            cycles = optical_mailbox.cycles;
            channel = optical_mailbox.channel;
            max_value = optical_mailbox.max_value;
            pre_delay_ms = optical_mailbox.pre_delay_ms;
            watchdog_ms = optical_mailbox.watchdog_ms;
            table_count = optical_mailbox.table_count;
            table_repeats = optical_mailbox.table_repeats;
            table_dwell_us = optical_mailbox.table_dwell_us;
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
            } else if (mode == MODE_TABLE_SCAN) {
                all_off(&last_rgbw);
                completed = run_table(sequence, table_count, table_repeats,
                                      table_dwell_us, pre_delay_ms,
                                      &last_rgbw);
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
