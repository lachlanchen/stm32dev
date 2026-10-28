/*
 * Separate dual-LCD plus finite lamp-alignment cycle. Prior images unchanged.
 *
 * PB8=SCL, PB9=SDA, VCC=3V3, LDAC=3V3, common ground.
 * Topology 1: LCD1 on A/B of address 1, LCD2 on A/B of address 2.
 * Topology 2: LCD1 on A/B and LCD2 on C/D of the same addressed DAC.
 * Two physical same-address DACs can mirror topology 2, but LCD2 must then
 * physically use C/D. Never infer two devices from one bus acknowledgement.
 *
 * Boot scans only 0x60..0x67 and parks every responding DAC channel at 1.5 V.
 * Automatic opposite looping is armed only when exactly two addresses exist.
 * Otherwise outputs remain at zero differential until explicitly configured.
 * PA2/TIM5_CH3 supplies nominal 4 V RMS PWM for 5 s, then zero for >=3 s.
 * Lamp cycling requires an explicit host arm and is disarmed on boot/reset.
 * PA0/PA1/PA3/PA4/PA5 remain low. The two LCD envelopes are unchanged.
 */
#define main preserved_qyh_single_lcd_main
#include "qyh1123_mcp4728_black_test_main.c"
#undef main

enum {
    DUAL_BOOT = 0,
    DUAL_WAIT_ADDRESSES = 1,
    DUAL_READY_OFF = 2,
    DUAL_LOOPING = 3,
    DUAL_I2C_FAULT = 4,
    DUAL_BAD_TOPOLOGY = 5
};

volatile uint32_t dual_magic = 0x444C4131u; /* DLA1: not the LCD-only image */
volatile uint32_t dual_ready = 0u;
volatile uint32_t dual_status = DUAL_BOOT;
volatile uint32_t dual_count = 0u;
volatile uint32_t dual_addresses[8] = {0};
volatile uint32_t dual_enable = 0u;
volatile uint32_t dual_topology = 1u;
volatile uint32_t dual_restart = 0u;
volatile uint32_t dual_rise_ms = 1000u;
volatile uint32_t dual_amplitude1_mv = 0u;
volatile uint32_t dual_amplitude2_mv = 0u;
volatile uint32_t dual_updates = 0u;
volatile uint32_t dual_ack_failures = 0u;
volatile uint32_t dual_late_halves = 0u;
volatile uint32_t dual_heartbeat = 0u;
volatile uint32_t dual_cycles = 0u;
volatile uint32_t dual_phase_ms = 0u;
volatile uint32_t dual_carrier_positive = 0u;
volatile uint32_t dual_lamp_enable = 0u;
volatile uint32_t dual_lamp_restart = 0u;
volatile uint32_t dual_lamp_on = 0u;
volatile uint32_t dual_lamp_duty = 0u;
volatile uint32_t dual_lamp_phase_ms = 0u;
volatile uint32_t dual_lamp_cycles = 0u;
volatile uint32_t dual_lamp_target_mv = 4000u;
volatile uint32_t dual_lamp_supply_bound_mv = 12500u;
volatile uint32_t dual_lamp_on_ms = 5000u;
volatile uint32_t dual_lamp_cool_ms = 3000u;

static void dual_lamp_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();
    TIM5->CR1 = 0u;
    TIM5->PSC = 6u;
    TIM5->ARR = 65535u;
    TIM5->CCR3 = 0u;
    TIM5->CCMR2 = (TIM5->CCMR2 & ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S))
        | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
    TIM5->CCER = (TIM5->CCER & ~(TIM_CCER_CC3P | TIM_CCER_CC3NP)) | TIM_CCER_CC3E;
    TIM5->EGR = TIM_EGR_UG;
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF2_TIM5;
    HAL_GPIO_Init(GPIOA, &gpio);
    TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void dual_lamp_tick(void)
{
    static uint32_t armed = 0u, start = 0u;
    if (!dual_lamp_enable || !dual_enable || !dual_ready || dual_ack_failures) {
        TIM5->CCR3 = 0u;
        dual_lamp_duty = 0u;
        dual_lamp_on = 0u;
        armed = 0u;
        return;
    }
    const uint32_t now = HAL_GetTick();
    if (!armed || dual_lamp_restart) {
        start = now;
        dual_lamp_restart = 0u;
        dual_lamp_cycles = 0u;
        armed = 1u;
    }
    uint32_t on_ms = dual_lamp_on_ms;
    uint32_t cool_ms = dual_lamp_cool_ms;
    if (on_ms > 5000u) { on_ms = 5000u; }
    if (cool_ms < 3000u) { cool_ms = 3000u; }
    if (cool_ms > 60000u) { cool_ms = 60000u; }
    const uint32_t duration = on_ms+cool_ms;
    const uint32_t elapsed = now-start;
    dual_lamp_phase_ms = elapsed % duration;
    dual_lamp_cycles = elapsed / duration;
    uint32_t target = dual_lamp_target_mv;
    uint32_t supply = dual_lamp_supply_bound_mv;
    if (target > 4000u) { target = 4000u; }
    if (supply < 12500u) { supply = 12500u; }
    const uint32_t duty = (uint32_t)(((uint64_t)target*target*65536u)/
                                    ((uint64_t)supply*supply));
    dual_lamp_on = (dual_lamp_phase_ms < on_ms && duty > 0u) ? 1u : 0u;
    dual_lamp_duty = dual_lamp_on ? duty : 0u;
    TIM5->CCR3 = dual_lamp_duty;
}

static void dual_watchdog_start(void)
{
    IWDG1->KR = 0x5555u;
    IWDG1->PR = 6u;
    IWDG1->RLR = 250u; /* Approximately 2 seconds with a nominal 32 kHz LSI. */
    IWDG1->KR = 0xAAAAu;
    IWDG1->KR = 0xCCCCu;
}

static bool dual_park_all(void)
{
    bool ok = true;
    const uint16_t middle = LCD_CODE_HIGH / 2u;
    for (uint32_t i = 0; i < dual_count; ++i) {
        for (uint8_t ch = 0; ch < 4u; ++ch) {
            ok = mcp4728_stage_channel((uint8_t)dual_addresses[i], ch, middle) && ok;
        }
    }
    if (dual_count) {
        ok = mcp4728_commit_all() && ok;
    }
    dual_amplitude1_mv = 0u;
    dual_amplitude2_mv = 0u;
    dual_carrier_positive = 0u;
    return ok;
}

static uint16_t dual_amplitude(uint32_t phase_ms, uint32_t rise_ms)
{
    const uint32_t distance = phase_ms <= rise_ms ? phase_ms : 2u*rise_ms-phase_ms;
    const uint32_t p = (distance*1000u)/rise_ms;
    const uint64_t smooth = (uint64_t)p*p*(3000u-2u*p);
    return (uint16_t)((3000u*smooth+500000000u)/1000000000u);
}

static bool dual_stage_pair(uint8_t address, uint8_t first,
                            uint16_t amplitude_mv, bool positive)
{
    const uint16_t magnitude = LCD_MV_TO_CODE(amplitude_mv);
    const uint16_t upper = (LCD_CODE_HIGH+magnitude)/2u;
    const uint16_t lower = LCD_CODE_HIGH-upper;
    const bool a = mcp4728_stage_channel(address, first, positive ? upper : lower);
    const bool b = mcp4728_stage_channel(address, first+1u, positive ? lower : upper);
    return a && b;
}

static bool dual_drive(uint16_t one, uint16_t two, bool positive)
{
    bool first, second;
    if (dual_topology == 1u && dual_count == 2u) {
        first = dual_stage_pair((uint8_t)dual_addresses[0], 0u, one, positive);
        second = dual_stage_pair((uint8_t)dual_addresses[1], 0u, two, positive);
    } else if (dual_topology == 2u && dual_count >= 1u) {
        first = dual_stage_pair((uint8_t)dual_addresses[0], 0u, one, positive);
        second = dual_stage_pair((uint8_t)dual_addresses[0], 2u, two, positive);
    } else {
        dual_status = DUAL_BAD_TOPOLOGY;
        return false;
    }
    if (!first || !second) {
        return false;
    }
    return mcp4728_commit_all();
}

int main(void)
{
    SCB_EnableICache();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4);
    delay_init(400);
    optical_outputs_force_safe();
    dual_lamp_init();
    i2c_bus_init();
    /* Retain the identity even when link-time section garbage collection runs. */
    if (dual_magic != 0x444C4131u) {
        while (1) { optical_outputs_force_safe(); }
    }
    for (uint8_t address = 0x60u; address <= 0x67u; ++address) {
        if (i2c_present(address)) {
            dual_addresses[dual_count++] = address;
        }
    }
    if (!dual_park_all()) {
        ++dual_ack_failures;
        dual_status = DUAL_I2C_FAULT;
    } else if (dual_count == 2u) {
        dual_status = DUAL_READY_OFF;
        dual_enable = 1u;
        dual_restart = 1u;
    } else {
        dual_status = DUAL_WAIT_ADDRESSES;
    }
    dual_ready = 1u;
    dual_watchdog_start();

    uint32_t start_ms = HAL_GetTick(), next_ms = start_ms;
    uint32_t half = 0u;
    uint32_t previous_enable = 0u;
    uint16_t one = 0u, two = 0u;
    while (1) {
        IWDG1->KR = 0xAAAAu;
        ++dual_heartbeat;
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 |
                          GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
        dual_lamp_tick();
        if (!dual_enable || dual_ack_failures) {
            if (previous_enable) {
                if (!dual_park_all()) { ++dual_ack_failures; }
                previous_enable = 0u;
                if (!dual_ack_failures) { dual_status = DUAL_READY_OFF; }
            }
            delay_ms(1);
            continue;
        }
        if ((dual_topology == 1u && dual_count != 2u) ||
            (dual_topology == 2u && dual_count < 1u) ||
            (dual_topology != 1u && dual_topology != 2u)) {
            dual_enable = 0u;
            dual_status = DUAL_BAD_TOPOLOGY;
            (void)dual_park_all();
            previous_enable = 0u;
            continue;
        }
        if (!previous_enable || dual_restart) {
            start_ms = HAL_GetTick();
            next_ms = start_ms;
            half = 0u;
            dual_cycles = 0u;
            dual_restart = 0u;
            previous_enable = 1u;
        }
        uint32_t now = HAL_GetTick();
        if ((int32_t)(now-next_ms) < 0) { continue; }
        if ((int32_t)(now-next_ms) > 2) { ++dual_late_halves; }
        uint32_t rise = dual_rise_ms;
        if (rise < 250u) { rise = 250u; }
        if (rise > 10000u) { rise = 10000u; }
        const bool positive = (half & 1u) == 0u;
        if (positive) {
            const uint32_t elapsed = now-start_ms;
            dual_phase_ms = elapsed % (2u*rise);
            dual_cycles = elapsed / (2u*rise);
            one = dual_amplitude(dual_phase_ms, rise);
            two = 3000u-one;
        }
        /* Hold amplitudes identical for the + and - halves of each AC pair. */
        if (!dual_drive(one, two, positive)) {
            ++dual_ack_failures;
            dual_status = DUAL_I2C_FAULT;
            dual_enable = 0u;
            (void)dual_park_all();
            previous_enable = 0u;
        } else {
            dual_status = DUAL_LOOPING;
            dual_amplitude1_mv = one;
            dual_amplitude2_mv = two;
            dual_carrier_positive = positive ? 1u : 0u;
            ++dual_updates;
        }
        ++half;
        next_ms += 5u; /* Nominal 100 Hz balanced carrier, not the optical scan rate. */
        if ((int32_t)(HAL_GetTick()-next_ms) >= 0) { next_ms = HAL_GetTick()+1u; }
    }
}
