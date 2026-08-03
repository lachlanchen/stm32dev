#include "main.h"
#include "sys.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * Brief, single-run 8 V RMS-equivalent overdrive for a 6 V tungsten lamp driven
 * from the measured 12.5 V supply through YYNMOS-1. The load still sees
 * 0/12.5 V pulses; 40.96% duty gives approximately 8.0 V RMS; reverse immediately.
 */
#define CORE_CLOCK_HZ              400000000u
#define TIM5_CLOCK_HZ              200000000u
#define PWM_FREQUENCY_HZ           443u
#define PWM_PERIOD_TICKS           ((TIM5_CLOCK_HZ + PWM_FREQUENCY_HZ / 2u) / PWM_FREQUENCY_HZ)
#define DUTY_TARGET                26843u
#define ASSUMED_SUPPLY_MV          12500u
#define EQUIVALENT_RMS_MV          8000u
#define STARTUP_OFF_MS             1000u
#define RAMP_MS                    1500u
#define SAMPLE_COUNT               1u
#define SAMPLE_INTERVAL_MS         0u
#define INA219_SHUNT_MOHM          100u
#define MONITOR_MAGIC              0x3656524Du

#define LAMP_PORT                  GPIOA
#define LAMP_PIN                   GPIO_PIN_2
#define PIXEL_PORT                 GPIOA
#define PIXEL_PIN                  GPIO_PIN_3
#define BB_PORT                    GPIOB
#define BB_SCL_PIN                 GPIO_PIN_8
#define BB_SDA_PIN                 GPIO_PIN_9

#define PIXEL_OFF_BITS             64u
#define PIXEL_T0H_CYCLES           120u
#define PIXEL_BIT_CYCLES           500u
#define PIXEL_RESET_US             300u

typedef struct {
    uint32_t magic;
    uint32_t present_mask;
    uint32_t duty_target;
    uint32_t assumed_supply_mV;
    uint32_t equivalent_rms_mV;
    uint32_t sequence_done;
    uint32_t valid_samples[4];
    uint32_t average_bus_mV[4];
    uint32_t average_current_mA[4];
    uint32_t average_power_mW[4];
    uint32_t minimum_current_mA[4];
    uint32_t maximum_current_mA[4];
    uint32_t minimum_power_mW[4];
    uint32_t maximum_power_mW[4];
} MonitorReport;

volatile MonitorReport monitor_report;
static uint32_t bus_sum[4];
static uint32_t current_sum[4];
static uint32_t power_sum[4];

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    TIM5->CCR3 = 0u;
    TIM5->CCER &= ~TIM_CCER_CC3E;
    LAMP_PORT->BSRRH = LAMP_PIN;
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    __disable_irq();
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

static void pixel_data_init_low(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    gpio.Pin = PIXEL_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(PIXEL_PORT, &gpio);
}

static void pixel_off(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    for (uint32_t bit = 0u; bit < PIXEL_OFF_BITS; ++bit) {
        uint32_t start = DWT->CYCCNT;
        PIXEL_PORT->BSRRL = PIXEL_PIN;
        wait_cycles(start, PIXEL_T0H_CYCLES);
        PIXEL_PORT->BSRRH = PIXEL_PIN;
        wait_cycles(start, PIXEL_BIT_CYCLES);
    }
    PIXEL_PORT->BSRRH = PIXEL_PIN;
    if (primask == 0u) __enable_irq();
    hold_us(PIXEL_RESET_US);
}

static void lamp_pwm_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = LAMP_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF2_TIM5;
    HAL_GPIO_Init(LAMP_PORT, &gpio);

    TIM5->CR1 = 0u;
    TIM5->PSC = 0u;
    TIM5->ARR = PWM_PERIOD_TICKS - 1u;
    TIM5->CCR3 = 0u;
    TIM5->CCMR2 = (TIM5->CCMR2 & ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S)) |
                  TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3PE;
    TIM5->CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC3P | TIM_CCER_CC3NP);
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0u;
    TIM5->CCER |= TIM_CCER_CC3E;
    TIM5->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void lamp_set_duty(uint16_t duty)
{
    uint32_t period = TIM5->ARR + 1u;
    TIM5->CCR3 = (uint32_t)(((uint64_t)duty * period + 32767u) / 65535u);
}

static void lamp_ramp(uint16_t from, uint16_t to, uint32_t duration_ms)
{
    int32_t delta = (int32_t)to - (int32_t)from;
    for (uint32_t step = 0u; step < duration_ms; ++step) {
        int32_t value = (int32_t)from;
        if (duration_ms > 1u) {
            value += (int32_t)(((int64_t)delta * step) / (int64_t)(duration_ms - 1u));
        }
        lamp_set_duty((uint16_t)value);
        hold_ms(1u);
    }
}

static void bb_delay(void)
{
    hold_us(5u);
}

static void bb_scl(bool high)
{
    HAL_GPIO_WritePin(BB_PORT, BB_SCL_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void bb_sda(bool high)
{
    HAL_GPIO_WritePin(BB_PORT, BB_SDA_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool bb_read_sda(void)
{
    return HAL_GPIO_ReadPin(BB_PORT, BB_SDA_PIN) == GPIO_PIN_SET;
}

static void i2c_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = BB_SCL_PIN | BB_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BB_PORT, &gpio);
    bb_sda(true);
    bb_scl(true);
    bb_delay();
}

static void bb_start(void)
{
    bb_sda(true); bb_scl(true); bb_delay();
    bb_sda(false); bb_delay();
    bb_scl(false); bb_delay();
}

static void bb_stop(void)
{
    bb_sda(false); bb_delay();
    bb_scl(true); bb_delay();
    bb_sda(true); bb_delay();
}

static bool bb_write(uint8_t value)
{
    for (uint32_t i = 0u; i < 8u; ++i) {
        bb_sda((value & 0x80u) != 0u);
        bb_delay(); bb_scl(true); bb_delay(); bb_scl(false); bb_delay();
        value <<= 1;
    }
    bb_sda(true); bb_delay(); bb_scl(true); bb_delay();
    bool ack = !bb_read_sda();
    bb_scl(false); bb_delay();
    return ack;
}

static uint8_t bb_read(bool acknowledge)
{
    uint8_t value = 0u;
    bb_sda(true);
    for (uint32_t i = 0u; i < 8u; ++i) {
        value <<= 1;
        bb_scl(true); bb_delay();
        if (bb_read_sda()) value |= 1u;
        bb_scl(false); bb_delay();
    }
    bb_sda(!acknowledge); bb_delay(); bb_scl(true); bb_delay();
    bb_scl(false); bb_delay(); bb_sda(true); bb_delay();
    return value;
}

static bool i2c_present(uint8_t address)
{
    bb_start();
    bool ok = bb_write((uint8_t)(address << 1));
    bb_stop();
    return ok;
}

static bool i2c_write16_be(uint8_t address, uint8_t reg, uint16_t value)
{
    bb_start();
    bool ok = bb_write((uint8_t)(address << 1));
    ok = bb_write(reg) && ok;
    ok = bb_write((uint8_t)(value >> 8)) && ok;
    ok = bb_write((uint8_t)value) && ok;
    bb_stop();
    return ok;
}

static bool i2c_read16_be(uint8_t address, uint8_t reg, uint16_t *value)
{
    bb_start();
    bool ok = bb_write((uint8_t)(address << 1));
    ok = bb_write(reg) && ok;
    bb_start();
    ok = bb_write((uint8_t)((address << 1) | 1u)) && ok;
    uint8_t high = 0u;
    uint8_t low = 0u;
    if (ok) {
        high = bb_read(true);
        low = bb_read(false);
    }
    bb_stop();
    *value = ((uint16_t)high << 8) | low;
    return ok;
}

static bool ina219_read(uint8_t address, uint32_t *bus_mV,
                        uint32_t *current_mA, uint32_t *power_mW)
{
    uint16_t raw_shunt_u = 0u;
    uint16_t raw_bus = 0u;
    if (!i2c_read16_be(address, 0x01u, &raw_shunt_u)) return false;
    if (!i2c_read16_be(address, 0x02u, &raw_bus)) return false;
    int32_t shunt_uV = (int32_t)(int16_t)raw_shunt_u * 10;
    if (shunt_uV < 0) shunt_uV = -shunt_uV;
    *bus_mV = ((uint32_t)(raw_bus >> 3) * 4u);
    *current_mA = (uint32_t)shunt_uV / INA219_SHUNT_MOHM;
    *power_mW = (*bus_mV * *current_mA) / 1000u;
    return *bus_mV <= 32000u && *current_mA <= 10000u;
}

static void report_init(void)
{
    memset((void *)&monitor_report, 0, sizeof(monitor_report));
    memset(bus_sum, 0, sizeof(bus_sum));
    memset(current_sum, 0, sizeof(current_sum));
    memset(power_sum, 0, sizeof(power_sum));
    monitor_report.magic = MONITOR_MAGIC;
    monitor_report.duty_target = DUTY_TARGET;
    monitor_report.assumed_supply_mV = ASSUMED_SUPPLY_MV;
    monitor_report.equivalent_rms_mV = EQUIVALENT_RMS_MV;
    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        monitor_report.minimum_current_mA[channel] = UINT32_MAX;
        monitor_report.minimum_power_mW[channel] = UINT32_MAX;
    }
}

static void monitors_detect_and_configure(void)
{
    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        uint8_t address = (uint8_t)(0x40u + channel);
        if (i2c_present(address) && i2c_write16_be(address, 0x00u, 0x3FFFu)) {
            monitor_report.present_mask |= 1u << channel;
        }
    }
}

static void monitors_sample(void)
{
    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        if ((monitor_report.present_mask & (1u << channel)) == 0u) continue;
        uint32_t bus_mV = 0u, current_mA = 0u, power_mW = 0u;
        if (!ina219_read((uint8_t)(0x40u + channel), &bus_mV, &current_mA, &power_mW)) continue;
        ++monitor_report.valid_samples[channel];
        bus_sum[channel] += bus_mV;
        current_sum[channel] += current_mA;
        power_sum[channel] += power_mW;
        if (current_mA < monitor_report.minimum_current_mA[channel]) monitor_report.minimum_current_mA[channel] = current_mA;
        if (current_mA > monitor_report.maximum_current_mA[channel]) monitor_report.maximum_current_mA[channel] = current_mA;
        if (power_mW < monitor_report.minimum_power_mW[channel]) monitor_report.minimum_power_mW[channel] = power_mW;
        if (power_mW > monitor_report.maximum_power_mW[channel]) monitor_report.maximum_power_mW[channel] = power_mW;
    }
}

static void report_finalize(void)
{
    for (uint32_t channel = 0u; channel < 4u; ++channel) {
        uint32_t count = monitor_report.valid_samples[channel];
        if (count != 0u) {
            monitor_report.average_bus_mV[channel] = bus_sum[channel] / count;
            monitor_report.average_current_mA[channel] = current_sum[channel] / count;
            monitor_report.average_power_mW[channel] = power_sum[channel] / count;
        } else {
            monitor_report.minimum_current_mA[channel] = 0u;
            monitor_report.minimum_power_mW[channel] = 0u;
        }
    }
    monitor_report.sequence_done = 1u;
    uintptr_t start = ((uintptr_t)&monitor_report) & ~(uintptr_t)31u;
    uintptr_t end = ((uintptr_t)&monitor_report + sizeof(monitor_report) + 31u) & ~(uintptr_t)31u;
    SCB_CleanDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
}

int main(void)
{
    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160u, 5u, 2u, 4u);
    dwt_init();
    pixel_data_init_low();
    pixel_off();
    lamp_pwm_init();
    i2c_init();
    report_init();
    monitors_detect_and_configure();

    lamp_set_duty(0u);
    hold_ms(STARTUP_OFF_MS);
    lamp_ramp(0u, DUTY_TARGET, RAMP_MS);
    lamp_set_duty(DUTY_TARGET);
    for (uint32_t sample = 0u; sample < SAMPLE_COUNT; ++sample) {
        monitors_sample();
        hold_ms(SAMPLE_INTERVAL_MS);
    }
    lamp_ramp(DUTY_TARGET, 0u, RAMP_MS);
    lamp_set_duty(0u);
    report_finalize();

    while (1) {
        lamp_set_duty(0u);
        hold_ms(1000u);
    }
}
