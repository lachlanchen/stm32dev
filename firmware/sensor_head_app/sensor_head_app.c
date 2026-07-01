/*
 * STM32H743IIT6 sensor-head app scaffold.
 *
 * Purpose:
 *   - AS7343 + TSL2591 measurement only.
 *   - Arduino remains lamp-control device.
 *   - Acquisition timing is separated from display drawing.
 *
 * This file is a portable architecture scaffold. Integrate these functions
 * into a CubeMX/HAL project or the vendor LCD example once ST-Link/USB serial
 * is visible on Windows.
 */

#include <stdint.h>
#include <stdbool.h>

#define AS7343_I2C_ADDR   0x39u
#define TSL2591_I2C_ADDR  0x29u
#define SENSOR_RING_SIZE  512u

typedef struct {
    uint32_t us;
    uint32_t seq;
    uint16_t tsl_ch0;
    uint16_t tsl_ch1;
    uint16_t tsl_visible;
    uint16_t as415;
    uint16_t as445;
    uint16_t as480;
    uint16_t as515;
    uint16_t as555;
    uint16_t as590;
    uint16_t as630;
    uint16_t as680;
    uint16_t asnir;
    uint16_t asclear;
} SensorFrame;

static volatile SensorFrame g_ring[SENSOR_RING_SIZE];
static volatile uint32_t g_write_index = 0;
static volatile uint32_t g_sequence = 0;

/* Platform hooks to implement in HAL/LL project. */
extern uint32_t platform_micros(void);
extern bool i2c_write8(uint8_t addr7, uint8_t reg, uint8_t value);
extern bool i2c_read16_le(uint8_t addr7, uint8_t reg, uint16_t *value);
extern bool as7343_read_channels(SensorFrame *frame);
extern void usb_or_uart_write_csv_frame(const SensorFrame *frame);
extern void lcd_draw_startup_image_aya_lala_sasa(void);
extern void lcd_plot_latest_spectrum_and_intensity(const SensorFrame *frame);

static bool tsl2591_init(void)
{
    const uint8_t TSL_CMD = 0xA0u;
    bool ok = true;
    ok = i2c_write8(TSL2591_I2C_ADDR, TSL_CMD | 0x00u, 0x03u) && ok; /* PON + AEN */
    ok = i2c_write8(TSL2591_I2C_ADDR, TSL_CMD | 0x01u, 0x00u) && ok; /* 100 ms, low gain */
    return ok;
}

static bool tsl2591_read(SensorFrame *frame)
{
    const uint8_t TSL_CMD = 0xA0u;
    uint16_t ch0 = 0;
    uint16_t ch1 = 0;
    if (!i2c_read16_le(TSL2591_I2C_ADDR, TSL_CMD | 0x14u, &ch0)) return false;
    if (!i2c_read16_le(TSL2591_I2C_ADDR, TSL_CMD | 0x16u, &ch1)) return false;
    frame->tsl_ch0 = ch0;
    frame->tsl_ch1 = ch1;
    frame->tsl_visible = (ch0 > ch1) ? (uint16_t)(ch0 - ch1) : 0;
    return true;
}

static void ring_push(const SensorFrame *frame)
{
    uint32_t idx = g_write_index % SENSOR_RING_SIZE;
    g_ring[idx] = *frame;
    g_write_index++;
}

static SensorFrame ring_latest(void)
{
    uint32_t wi = g_write_index;
    if (wi == 0) {
        SensorFrame empty = {0};
        return empty;
    }
    return g_ring[(wi - 1u) % SENSOR_RING_SIZE];
}

static void acquire_once(void)
{
    SensorFrame frame = {0};
    frame.us = platform_micros();
    frame.seq = g_sequence++;

    (void)tsl2591_read(&frame);
    (void)as7343_read_channels(&frame);

    ring_push(&frame);
    usb_or_uart_write_csv_frame(&frame);
}

int main(void)
{
    /*
     * Expected platform init:
     *   SystemClock_Config();
     *   I2C1 PB8/PB9 init, 400 kHz first.
     *   LCD/framebuffer init.
     *   USB CDC or UART init.
     */

    lcd_draw_startup_image_aya_lala_sasa();
    (void)tsl2591_init();

    uint32_t last_plot_us = 0;

    for (;;) {
        acquire_once();

        uint32_t now = platform_micros();
        if ((uint32_t)(now - last_plot_us) >= 50000u) {
            SensorFrame latest = ring_latest();
            lcd_plot_latest_spectrum_and_intensity(&latest);
            last_plot_us = now;
        }
    }
}

