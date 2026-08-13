/*
 * QYH1123 smooth complementary triangle test using an MCP4728.
 *
 * Wiring:
 *   STM32 PB8  -> MCP4728 SCL
 *   STM32 PB9  -> MCP4728 SDA
 *   STM32 3V3  -> MCP4728 VCC and LDAC
 *   STM32 GND  -> MCP4728 GND
 *   MCP4728 A  -> 1 kohm -> QYH1123 electrode 1
 *   MCP4728 B  -> 1 kohm -> QYH1123 electrode 2
 *   STM32 PA4  -> NLED EN  (acquisition-gated illumination enable)
 *   STM32 PA5  -> NLED DIM (100 percent current command while enabled)
 *
 * During each sweep, A and B remain complementary while Vdiff scans
 * -3 V -> +3 V -> -3 V. A generic inverse TN/HTN electro-optic lookup table
 * makes estimated optical darkness, rather than voltage, vary linearly with
 * time. A+B stays at 3 V and the differential average over a complete cycle
 * is approximately zero.
 * LDAC may remain tied high because every pair of channel writes is committed
 * by the General Call Software Update command.
 *
 * This is a separate diagnostic image. It does not replace or edit the normal
 * sensor-head source. PA0..PA3 are held low to keep disconnected optical
 * control outputs safe. PA4 and PA5 default low and are enabled only during a
 * synchronized acquisition requested through the exported SWD control words.
 */

#include "sys.h"
#include "delay.h"
#include "usart.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MCP4728_ADDR_FIRST       0x60u
#define MCP4728_ADDR_LAST        0x67u
#define MCP4728_CMD_MULTI_WRITE  0x40u
#define MCP4728_GENERAL_UPDATE   0x08u

#define LCD_VDD_MV               3300u
#define LCD_OUTPUT_HIGH_MV       3000u
#define LCD_OUTPUT_LOW_MV        0u
#define LCD_SWEEP_ONE_WAY_MS     1000u
#define LCD_SWEEP_STEPS          1000u
#define LCD_MV_TO_CODE(mv)       ((uint16_t)(((uint32_t)(mv) * 4096u + (LCD_VDD_MV / 2u)) / LCD_VDD_MV))
#define LCD_CODE_HIGH            LCD_MV_TO_CODE(LCD_OUTPUT_HIGH_MV)
#define LCD_CODE_LOW             LCD_MV_TO_CODE(LCD_OUTPUT_LOW_MV)

#define BB_PORT                  GPIOB
#define BB_SCL_PIN               GPIO_PIN_8
#define BB_SDA_PIN               GPIO_PIN_9

enum {
    QYH_STATUS_BOOT = 0,
    QYH_STATUS_MCP_NOT_FOUND = 1,
    QYH_STATUS_CONFIG_FAILED = 2,
    QYH_STATUS_DRIVING = 3,
    QYH_STATUS_RUNTIME_I2C_ERROR = 4
};

volatile uint32_t qyh_magic = 0x51594831u; /* "QYH1" */
volatile uint32_t qyh_status = QYH_STATUS_BOOT;
volatile uint32_t qyh_mcp4728_address = 0u;
volatile uint32_t qyh_phase = 0u;
volatile uint32_t qyh_updates = 0u;
volatile uint32_t qyh_ack_failures = 0u;
volatile uint32_t qyh_code_high = LCD_CODE_HIGH;
volatile uint32_t qyh_code_a = LCD_CODE_LOW;
volatile uint32_t qyh_code_b = LCD_CODE_HIGH;
volatile uint32_t qyh_sweep_position = 0u;
volatile int32_t qyh_sweep_direction = 1;
volatile uint32_t qyh_target_darkness_permille = 1000u;
volatile uint32_t qyh_drive_amplitude_mv = 3000u;
volatile uint32_t qyh_led_enable = 0u;
volatile uint32_t qyh_lcd_enable = 1u;
volatile uint32_t qyh_phase_restart = 0u;
volatile uint32_t qyh_capture_cycles = 0u;
volatile uint32_t qyh_capture_completed = 0u;

/*
 * Generic inverse electro-optic curve for a normally-white TN/HTN cell.
 * The index is desired darkness in 1/16 increments; values are differential
 * voltage magnitude in mV. This is deliberately a smooth heuristic, not a
 * substitute for a measured transmission-versus-voltage calibration.
 */
static const uint16_t lcd_inverse_darkness_mv[17] = {
       0u,  700u,  950u, 1100u, 1220u, 1320u, 1410u, 1500u, 1580u,
    1660u, 1750u, 1840u, 1940u, 2060u, 2220u, 2470u, 3000u
};

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}

static void optical_outputs_force_safe(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3,
                      GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
               GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static void constant_led_set(bool enabled)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5,
                      enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void bb_delay(void)
{
    delay_us(2);
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

static void i2c_bus_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = BB_SCL_PIN | BB_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BB_PORT, &gpio);

    bb_sda(true);
    bb_scl(true);
    bb_delay();
}

static void bb_start(void)
{
    bb_sda(true);
    bb_scl(true);
    bb_delay();
    bb_sda(false);
    bb_delay();
    bb_scl(false);
}

static void bb_stop(void)
{
    bb_sda(false);
    bb_delay();
    bb_scl(true);
    bb_delay();
    bb_sda(true);
    bb_delay();
}

static bool bb_write_byte(uint8_t value)
{
    for (uint32_t bit = 0; bit < 8u; ++bit) {
        bb_sda((value & 0x80u) != 0u);
        bb_delay();
        bb_scl(true);
        bb_delay();
        bb_scl(false);
        value <<= 1;
    }

    bb_sda(true);
    bb_delay();
    bb_scl(true);
    bb_delay();
    const bool acknowledged = !bb_read_sda();
    bb_scl(false);
    return acknowledged;
}

static bool i2c_present(uint8_t address)
{
    bb_start();
    const bool acknowledged = bb_write_byte((uint8_t)(address << 1));
    bb_stop();
    return acknowledged;
}

static uint8_t mcp4728_find(void)
{
    for (uint8_t address = MCP4728_ADDR_FIRST; address <= MCP4728_ADDR_LAST; ++address) {
        if (i2c_present(address)) {
            return address;
        }
    }
    return 0u;
}

static bool mcp4728_stage_channel(uint8_t address, uint8_t channel, uint16_t code)
{
    bool ok = true;

    code &= 0x0FFFu;
    bb_start();
    ok = bb_write_byte((uint8_t)(address << 1)) && ok;
    /* 01000 DAC1 DAC0 UDAC: UDAC=1 holds the output until software update. */
    ok = bb_write_byte((uint8_t)(MCP4728_CMD_MULTI_WRITE |
                                 ((channel & 0x03u) << 1) | 0x01u)) && ok;
    /* VREF=VDD, normal power, gain=1, followed by D11..D8. */
    ok = bb_write_byte((uint8_t)((code >> 8) & 0x0Fu)) && ok;
    ok = bb_write_byte((uint8_t)(code & 0xFFu)) && ok;
    bb_stop();
    return ok;
}

static bool mcp4728_commit_all(void)
{
    bool ok = true;

    bb_start();
    ok = bb_write_byte(0x00u) && ok; /* I2C General Call write address. */
    ok = bb_write_byte(MCP4728_GENERAL_UPDATE) && ok;
    bb_stop();
    return ok;
}

static bool mcp4728_set_pair(uint8_t address, uint16_t code_a, uint16_t code_b)
{
    const bool a_ok = mcp4728_stage_channel(address, 0u, code_a);
    const bool b_ok = mcp4728_stage_channel(address, 1u, code_b);
    const bool update_ok = mcp4728_commit_all();
    return a_ok && b_ok && update_ok;
}

static uint16_t lcd_voltage_for_darkness(uint16_t darkness_permille)
{
    if (darkness_permille >= 1000u) {
        return lcd_inverse_darkness_mv[16];
    }

    const uint32_t scaled = (uint32_t)darkness_permille * 16u;
    const uint32_t index = scaled / 1000u;
    const uint32_t fraction = scaled % 1000u;
    const uint32_t low = lcd_inverse_darkness_mv[index];
    const uint32_t high = lcd_inverse_darkness_mv[index + 1u];
    return (uint16_t)(low + ((high - low) * fraction + 500u) / 1000u);
}

static void dwt_timer_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void wait_until_cycle(uint32_t deadline)
{
    while ((int32_t)(DWT->CYCCNT - deadline) < 0) {
    }
}

int main(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4);
    delay_init(400);
    uart_init(115200);
    optical_outputs_force_safe();
    i2c_bus_init();
    dwt_timer_init();

    printf("\r\nQYH1123 MCP4728 smooth complementary triangle test\r\n");
    printf("PB8=SCL PB9=SDA; LDAC=3V3; heuristic uniform-intensity sweep\r\n");
    printf("Vdiff=-3V..+3V, %lu ms each way, inverse TN/HTN response LUT\r\n",
           (unsigned long)LCD_SWEEP_ONE_WAY_MS);

    const uint8_t address = mcp4728_find();
    qyh_mcp4728_address = address;
    if (address == 0u) {
        qyh_status = QYH_STATUS_MCP_NOT_FOUND;
        printf("ERROR: MCP4728 not found at 0x60..0x67\r\n");
        while (1) {
            delay_ms(250);
        }
    }

    printf("MCP4728 detected at 0x%02X; high code=%lu\r\n",
           address, (unsigned long)LCD_CODE_HIGH);

    if (!mcp4728_set_pair(address, LCD_CODE_LOW, LCD_CODE_HIGH)) {
        qyh_status = QYH_STATUS_CONFIG_FAILED;
        ++qyh_ack_failures;
        printf("ERROR: initial MCP4728 write failed\r\n");
        while (1) {
            delay_ms(250);
        }
    }

    qyh_status = QYH_STATUS_DRIVING;
    const uint32_t step_period_cycles =
        (SystemCoreClock / 1000u) * LCD_SWEEP_ONE_WAY_MS / LCD_SWEEP_STEPS;
    uint32_t deadline = DWT->CYCCNT + step_period_cycles;
    uint32_t position = 0u;
    int32_t direction = 1;

    while (1) {
        if (qyh_lcd_enable == 0u) {
            const uint16_t parked_code = LCD_MV_TO_CODE(LCD_OUTPUT_HIGH_MV / 2u);
            constant_led_set(qyh_led_enable != 0u);
            if (qyh_code_a != parked_code || qyh_code_b != parked_code) {
                if (mcp4728_set_pair(address, parked_code, parked_code)) {
                    qyh_code_a = parked_code;
                    qyh_code_b = parked_code;
                    qyh_drive_amplitude_mv = 0u;
                    qyh_target_darkness_permille = 0u;
                    qyh_sweep_position = LCD_SWEEP_STEPS / 2u;
                    qyh_sweep_direction = 0;
                } else {
                    ++qyh_ack_failures;
                    qyh_status = QYH_STATUS_RUNTIME_I2C_ERROR;
                }
            }
            delay_ms(1);
            continue;
        }

        if (qyh_phase_restart != 0u) {
            constant_led_set(false);
            (void)mcp4728_set_pair(address, LCD_CODE_LOW, LCD_CODE_HIGH);
            position = 0u;
            direction = 1;
            qyh_sweep_position = 0u;
            qyh_sweep_direction = 1;
            qyh_capture_completed = 0u;
            qyh_phase_restart = 0u;
            deadline = DWT->CYCCNT + step_period_cycles;
            constant_led_set(qyh_led_enable != 0u);
            continue;
        }

        constant_led_set(qyh_led_enable != 0u);
        wait_until_cycle(deadline);
        deadline += step_period_cycles;

        const int32_t centered = (int32_t)(2u * position) - (int32_t)LCD_SWEEP_STEPS;
        const uint32_t distance = (uint32_t)(centered < 0 ? -centered : centered);
        const uint16_t darkness_permille = (uint16_t)
            ((distance * 1000u + (LCD_SWEEP_STEPS / 2u)) / LCD_SWEEP_STEPS);
        const uint16_t amplitude_mv = lcd_voltage_for_darkness(darkness_permille);
        const int32_t differential_mv = centered < 0
            ? -(int32_t)amplitude_mv
            : (int32_t)amplitude_mv;
        const uint16_t output_a_mv = (uint16_t)
            (((int32_t)LCD_OUTPUT_HIGH_MV + differential_mv) / 2);
        const uint16_t output_b_mv = (uint16_t)
            (((int32_t)LCD_OUTPUT_HIGH_MV - differential_mv) / 2);
        const uint16_t code_a = LCD_MV_TO_CODE(output_a_mv);
        const uint16_t code_b = LCD_MV_TO_CODE(output_b_mv);

        const bool ok = mcp4728_set_pair(address, code_a, code_b);

        if (!ok) {
            ++qyh_ack_failures;
            qyh_status = QYH_STATUS_RUNTIME_I2C_ERROR;
        } else {
            qyh_status = QYH_STATUS_DRIVING;
            ++qyh_updates;
            qyh_code_a = code_a;
            qyh_code_b = code_b;
            qyh_sweep_position = position;
            qyh_sweep_direction = direction;
            qyh_target_darkness_permille = darkness_permille;
            qyh_drive_amplitude_mv = amplitude_mv;
            qyh_phase = direction > 0 ? 1u : 0u;
        }

        if (direction > 0) {
            if (position >= LCD_SWEEP_STEPS) {
                direction = -1;
                --position;
            } else {
                ++position;
            }
        } else if (position == 0u) {
            if (qyh_led_enable != 0u && qyh_capture_cycles != 0u) {
                ++qyh_capture_completed;
                if (qyh_capture_completed >= qyh_capture_cycles) {
                    qyh_led_enable = 0u;
                    constant_led_set(false);
                }
            }
            direction = 1;
            ++position;
        } else {
            --position;
        }

        if ((int32_t)(DWT->CYCCNT - deadline) >= 0) {
            deadline = DWT->CYCCNT + step_period_cycles;
        }
    }
}
