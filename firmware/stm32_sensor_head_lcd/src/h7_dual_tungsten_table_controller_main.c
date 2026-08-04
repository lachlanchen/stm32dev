#include "main.h"
#include "sys.h"
#include <stdint.h>
#include <string.h>

/*
 * Fail-safe host-armed controller for two tungsten lamps.
 *
 * PA1/A1 = TIM5_CH2
 * PA2/A2 = TIM5_CH3
 *
 * A host uploads a finite table of (A1, A2) logical 16-bit duties through
 * SWD. Both compare registers use one TIM5 counter and preload on the same
 * timer update. Every completed, cancelled, invalid, or failed command ends
 * with both outputs at zero. No autonomous loop exists.
 *
 * The mailbox layout intentionally matches h7_optical_table_control.py. The
 * old field names "tungsten" and "rgbw" are interpreted here as A1 and A2.
 */

#define CORE_CLOCK_HZ                 400000000u
#define TIM5_CLOCK_HZ                 200000000u
#define LAMP_PWM_HZ                   443u
#define LAMP_PERIOD_TICKS             ((TIM5_CLOCK_HZ + LAMP_PWM_HZ / 2u) / LAMP_PWM_HZ)
#define LAMP_PORT                     GPIOA
#define LAMP_PINS                     (GPIO_PIN_1 | GPIO_PIN_2)

#define MAILBOX_MAGIC                 0x4f505443u
#define MAILBOX_VERSION               2u
#define MODE_OFF                      0u
#define MODE_TABLE_SCAN               5u
#define STATUS_READY                  0u
#define STATUS_RUNNING                1u
#define STATUS_CANCELLED              4u
#define OPTICAL_SCHEDULE_CAPACITY     4096u
#define MIN_TABLE_DWELL_US            500u
#define MAX_TABLE_DWELL_US            1000000u
#define MAX_TABLE_REPEATS             100u

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
    uint32_t direct_a1;
    uint32_t direct_a2;
    uint32_t watchdog_ms;
    uint32_t status;
    uint32_t applied_a1;
    uint32_t applied_a2;
    uint32_t sample_index;
    uint32_t total_samples;
    uint32_t heartbeat;
    uint32_t table_count;
    uint32_t table_repeats;
    uint32_t table_dwell_us;
} OpticalMailbox;

typedef struct {
    uint32_t a1;
    uint32_t a2;
} OpticalState;

volatile OpticalMailbox optical_mailbox __attribute__((used, aligned(32)));
volatile OpticalState optical_schedule[OPTICAL_SCHEDULE_CAPACITY]
    __attribute__((used, aligned(32)));

void SysTick_Handler(void)
{
    HAL_IncTick();
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

static void pwm_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();
    HAL_GPIO_WritePin(LAMP_PORT, LAMP_PINS, GPIO_PIN_RESET);
    gpio.Pin = LAMP_PINS;
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

static uint32_t compare_from_logical(uint32_t duty)
{
    uint32_t bounded = duty > 65535u ? 65535u : duty;
    uint32_t period = TIM5->ARR + 1u;
    return (uint32_t)(((uint64_t)bounded * period + 32767u) / 65535u);
}

static void sources_set(uint32_t a1, uint32_t a2)
{
    uint32_t bounded_a1 = a1 > 65535u ? 65535u : a1;
    uint32_t bounded_a2 = a2 > 65535u ? 65535u : a2;

    TIM5->CCR2 = compare_from_logical(bounded_a1);
    TIM5->CCR3 = compare_from_logical(bounded_a2);
    optical_mailbox.applied_a1 = bounded_a1;
    optical_mailbox.applied_a2 = bounded_a2;
}

static void all_off(void)
{
    TIM5->CCR2 = 0u;
    TIM5->CCR3 = 0u;
    TIM5->EGR = TIM_EGR_UG;
    optical_mailbox.applied_a1 = 0u;
    optical_mailbox.applied_a2 = 0u;
}

void Error_Handler(void)
{
    all_off();
    TIM5->CCER &= ~(TIM_CCER_CC2E | TIM_CCER_CC3E);
    __disable_irq();
    while (1) {
    }
}

static uint8_t run_table(uint32_t sequence, uint32_t count,
                         uint32_t repeats, uint32_t dwell_us,
                         uint32_t pre_delay_ms)
{
    uint32_t next_frame;
    uint32_t dwell_cycles;
    uint32_t sample = 0u;

    if (count == 0u || count > OPTICAL_SCHEDULE_CAPACITY ||
        repeats == 0u || repeats > MAX_TABLE_REPEATS ||
        dwell_us < MIN_TABLE_DWELL_US || dwell_us > MAX_TABLE_DWELL_US) {
        return 0u;
    }
    optical_mailbox.total_samples = count * repeats;
    optical_mailbox.sample_index = 0u;
    hold_ms(pre_delay_ms);
    next_frame = DWT->CYCCNT;
    dwell_cycles = (CORE_CLOCK_HZ / 1000000u) * dwell_us;

    for (uint32_t repeat = 0u; repeat < repeats; repeat++) {
        for (uint32_t index = 0u; index < count; index++) {
            uint32_t a1;
            uint32_t a2;

            if (optical_mailbox.command_seq != sequence) {
                return 0u;
            }
            __DMB();
            a1 = optical_schedule[index].a1;
            a2 = optical_schedule[index].a2;
            sources_set(a1, a2);
            optical_mailbox.sample_index = ++sample;
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

    /* Keep D-cache off so SWD table and mailbox writes are coherent. */
    SCB_EnableICache();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u);
    dwt_init();
    pwm_init();
    memset((void *)&optical_mailbox, 0, sizeof(optical_mailbox));
    optical_mailbox.magic = MAILBOX_MAGIC;
    optical_mailbox.version = MAILBOX_VERSION;
    optical_mailbox.status = STATUS_READY;
    all_off();
    __DMB();

    while (1) {
        uint32_t sequence = optical_mailbox.command_seq;

        if (sequence != handled_sequence) {
            uint32_t mode;
            uint32_t pre_delay_ms;
            uint32_t table_count;
            uint32_t table_repeats;
            uint32_t table_dwell_us;
            uint8_t completed = 0u;

            __DMB();
            mode = optical_mailbox.mode;
            pre_delay_ms = optical_mailbox.pre_delay_ms;
            table_count = optical_mailbox.table_count;
            table_repeats = optical_mailbox.table_repeats;
            table_dwell_us = optical_mailbox.table_dwell_us;
            handled_sequence = sequence;
            optical_mailbox.ack_seq = sequence;
            optical_mailbox.status = STATUS_RUNNING;
            optical_mailbox.sample_index = 0u;
            optical_mailbox.total_samples = 0u;
            __DMB();

            all_off();
            if (mode == MODE_TABLE_SCAN) {
                completed = run_table(sequence, table_count, table_repeats,
                                      table_dwell_us, pre_delay_ms);
            }
            all_off();
            optical_mailbox.status = completed != 0u ? STATUS_READY :
                                      (mode == MODE_OFF ? STATUS_READY : STATUS_CANCELLED);
            optical_mailbox.done_seq = sequence;
            __DMB();
        }
        optical_mailbox.heartbeat++;
        hold_us(100u);
    }
}
