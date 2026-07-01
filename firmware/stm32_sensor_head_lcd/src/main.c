#include "main.h"
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "lcd.h"
#include "sdram.h"
#include "startup_image.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define AS7343_ADDR          0x39u
#define TSL2591_ADDR         0x29u
#define TSL2591_CMD          0xA0u

#define AS_REG_CFG0          0xBFu
#define AS_REG_ENABLE        0x80u
#define AS_REG_ATIME         0x81u
#define AS_REG_STATUS2       0x90u
#define AS_REG_ID            0x5Au
#define AS_REG_DATA0         0x95u
#define AS_REG_CFG1          0xC6u
#define AS_REG_ASTEP_L       0xD4u
#define AS_REG_CFG20         0xD6u

#define AS_ATIME_FAST        5u
#define AS_ASTEP_FAST        599u
#define AS_FSR_FAST          ((uint32_t)(AS_ATIME_FAST + 1u) * (uint32_t)(AS_ASTEP_FAST + 1u))
#define AS_PERIOD_MS         12u
#define TSL_PERIOD_MS        100u
#define DRAW_PERIOD_MS       20u
#define AS_FIXED_VISIBLE_MAX AS_FSR_FAST
#define AS_FIXED_AUX_MAX     AS_FSR_FAST
#define TSL_FIXED_RAW_MAX    65535u
#define CAPTURE_LOG_MAGIC    0x53504C47u
#define CAPTURE_LOG_CAPACITY 2048u

#define UI_BG                0x0841u
#define UI_PANEL             0x18E3u
#define UI_GRID              0x39E7u
#define UI_TEXT              0xFFFFu
#define UI_GREEN             0x07E0u
#define UI_ORANGE            0xFD20u
#define UI_CYAN              0x07FFu
#define UI_RED               0xF800u
#define UI_YELLOW            0xFFE0u
#define UI_BLUE              0x041Fu

I2C_HandleTypeDef hi2c1;

static bool ok_tsl = false;
static bool ok_as7343 = false;
static uint16_t last_as[18];
static uint16_t last_tsl0 = 0;
static uint16_t last_tsl1 = 0;
static uint32_t last_tsl0_norm = 0;
static uint32_t last_tsl_visible_norm = 0;
static uint8_t last_status2 = 0;
static uint32_t seq = 0;

#define SPEC_CLEAR_AVG        0xFEu
#define SPEC_FD_AVG           0xFDu

static const uint8_t spec_ch[] = {
    12, 6, 0, 7, 8, 15, 1, 2, 9, 13, 14, 3, SPEC_CLEAR_AVG, SPEC_FD_AVG
};
#define SPEC_MAIN_COUNT       11u

static uint16_t intensity_x = 0;
static uint16_t prev_tsl_y = 0;
static uint16_t prev_full_y = 0;
static uint16_t prev_diag_y = 0;
static bool intensity_started = false;
static uint32_t as_trace_max = 100;
static uint32_t as_aux_bar_max = 100;
static uint32_t tsl_bar_max = 1024;
static uint32_t tsl_visible_baseline = 0;
static uint32_t tsl_full_baseline = 0;
static uint32_t tsl_visible_span = 16;
static uint32_t tsl_full_span = 16;
static uint16_t prev_spec_y[SPEC_MAIN_COUNT];
static bool spectrum_started = false;
static bool scale_fixed_mode = false;
static uint8_t tsl_gain_index = 1;
static uint8_t tsl_gain_code = 0x10;
static uint8_t as_gain_code = 5;
static uint32_t as_sample_count = 0;
static uint32_t tsl_sample_count = 0;

typedef struct {
    uint32_t t_ms;
    uint32_t seq;
    uint32_t as_samples;
    uint32_t tsl_samples;
    uint16_t tsl_ch0;
    uint16_t tsl_ch1;
    uint16_t tsl_visible;
    uint16_t tsl_gain;
    uint16_t ok_flags;
    uint16_t status2;
    uint16_t as_gain;
    uint16_t reserved;
    uint16_t ch[18];
} CaptureRecord;

volatile uint32_t capture_log_magic = CAPTURE_LOG_MAGIC;
volatile uint32_t capture_log_capacity = CAPTURE_LOG_CAPACITY;
volatile uint32_t capture_log_record_size = sizeof(CaptureRecord);
volatile uint32_t capture_log_write = 0;
volatile CaptureRecord capture_log[CAPTURE_LOG_CAPACITY];

static void capture_log_init_metadata(void)
{
    capture_log_magic = CAPTURE_LOG_MAGIC;
    capture_log_capacity = CAPTURE_LOG_CAPACITY;
    capture_log_record_size = sizeof(CaptureRecord);
    capture_log_write = 0;
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#define BB_PORT    GPIOB

static uint16_t bb_scl_pin = GPIO_PIN_8;
static uint16_t bb_sda_pin = GPIO_PIN_9;
static uint8_t i2c_mode = 0;

static void bb_delay(void)
{
    delay_us(5);
}

static void bb_scl(bool high)
{
    HAL_GPIO_WritePin(BB_PORT, bb_scl_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void bb_sda(bool high)
{
    HAL_GPIO_WritePin(BB_PORT, bb_sda_pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool bb_read_sda(void)
{
    return HAL_GPIO_ReadPin(BB_PORT, bb_sda_pin) == GPIO_PIN_SET;
}

static void i2c1_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = bb_scl_pin | bb_sda_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BB_PORT, &gpio);
    bb_sda(true);
    bb_scl(true);
    bb_delay();
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    (void)hi2c;
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

static bool bb_write_byte(uint8_t v)
{
    for (uint8_t i = 0; i < 8; i++) {
        bb_sda((v & 0x80u) != 0);
        bb_delay();
        bb_scl(true); bb_delay();
        bb_scl(false); bb_delay();
        v <<= 1;
    }
    bb_sda(true); bb_delay();
    bb_scl(true); bb_delay();
    bool ack = !bb_read_sda();
    bb_scl(false); bb_delay();
    return ack;
}

static uint8_t bb_read_byte(bool ack)
{
    uint8_t v = 0;
    bb_sda(true);
    for (uint8_t i = 0; i < 8; i++) {
        v <<= 1;
        bb_scl(true); bb_delay();
        if (bb_read_sda()) v |= 1u;
        bb_scl(false); bb_delay();
    }
    bb_sda(!ack); bb_delay();
    bb_scl(true); bb_delay();
    bb_scl(false); bb_delay();
    bb_sda(true); bb_delay();
    return v;
}

static bool i2c_present(uint8_t addr)
{
    bb_start();
    bool ok = bb_write_byte((uint8_t)(addr << 1));
    bb_stop();
    return ok;
}

static bool i2c_write8(uint8_t addr, uint8_t reg, uint8_t value)
{
    bb_start();
    bool ok = bb_write_byte((uint8_t)(addr << 1));
    ok = bb_write_byte(reg) && ok;
    ok = bb_write_byte(value) && ok;
    bb_stop();
    return ok;
}

static bool i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    bb_start();
    bool ok = bb_write_byte((uint8_t)(addr << 1));
    ok = bb_write_byte(reg) && ok;
    bb_start();
    ok = bb_write_byte((uint8_t)((addr << 1) | 1u)) && ok;
    if (ok) {
        for (uint16_t i = 0; i < len; i++) buf[i] = bb_read_byte(i + 1u < len);
    }
    bb_stop();
    return ok;
}

static bool i2c_read8(uint8_t addr, uint8_t reg, uint8_t *value)
{
    return i2c_read_bytes(addr, reg, value, 1);
}

static bool i2c_write16_le(uint8_t addr, uint8_t reg, uint16_t value)
{
    bb_start();
    bool ok = bb_write_byte((uint8_t)(addr << 1));
    ok = bb_write_byte(reg) && ok;
    ok = bb_write_byte((uint8_t)(value & 0xFFu)) && ok;
    ok = bb_write_byte((uint8_t)(value >> 8)) && ok;
    bb_stop();
    return ok;
}

static bool as_set_bank(bool bank1)
{
    uint8_t cfg0 = 0;
    if (!i2c_read8(AS7343_ADDR, AS_REG_CFG0, &cfg0)) return false;
    if (bank1) cfg0 |= 0x10u;
    else cfg0 &= (uint8_t)~0x10u;
    return i2c_write8(AS7343_ADDR, AS_REG_CFG0, cfg0);
}

static bool as_read_id(uint8_t *id)
{
    if (!as_set_bank(true)) return false;
    bool ok = i2c_read8(AS7343_ADDR, AS_REG_ID, id);
    as_set_bank(false);
    return ok;
}

static bool as7343_configure(void)
{
    uint8_t id = 0;
    if (!as_read_id(&id)) return false;
    if (id != 0x81u) return false;
    if (!as_set_bank(false)) return false;
    if (!i2c_write8(AS7343_ADDR, AS_REG_ENABLE, 0x01u)) return false;
    HAL_Delay(2);
    if (!i2c_write8(AS7343_ADDR, AS_REG_ATIME, AS_ATIME_FAST)) return false;
    if (!i2c_write16_le(AS7343_ADDR, AS_REG_ASTEP_L, AS_ASTEP_FAST)) return false;
    if (!i2c_write8(AS7343_ADDR, AS_REG_CFG1, as_gain_code)) return false; /* 16x initial gain */
    uint8_t cfg20 = 0;
    if (!i2c_read8(AS7343_ADDR, AS_REG_CFG20, &cfg20)) return false;
    cfg20 &= (uint8_t)~0x60u;
    cfg20 |= 0x60u; /* auto SMUX, 18-channel mode */
    if (!i2c_write8(AS7343_ADDR, AS_REG_CFG20, cfg20)) return false;
    if (!i2c_write8(AS7343_ADDR, AS_REG_ENABLE, 0x03u)) return false;
    HAL_Delay(AS_PERIOD_MS + 2u);
    return true;
}

static void as7343_auto_gain(const uint16_t *channels)
{
    (void)channels;
    /* Fixed gain keeps spectral samples comparable across time. */
}

static bool as7343_read18(uint16_t *channels, uint8_t *status2)
{
    uint8_t raw[36];
    if (!as_set_bank(false)) return false;
    i2c_read8(AS7343_ADDR, AS_REG_STATUS2, status2);
    if (!i2c_read_bytes(AS7343_ADDR, AS_REG_DATA0, raw, sizeof(raw))) return false;
    for (uint8_t i = 0; i < 18; i++) channels[i] = ((uint16_t)raw[2u * i + 1u] << 8) | raw[2u * i];
    as7343_auto_gain(channels);
    return true;
}

static bool tsl_write8(uint8_t reg, uint8_t value)
{
    return i2c_write8(TSL2591_ADDR, (uint8_t)(TSL2591_CMD | reg), value);
}

static bool tsl_read16(uint8_t reg, uint16_t *value)
{
    uint8_t raw[2];
    if (!i2c_read_bytes(TSL2591_ADDR, (uint8_t)(TSL2591_CMD | reg), raw, 2)) return false;
    *value = ((uint16_t)raw[1] << 8) | raw[0];
    return true;
}

static bool tsl2591_configure(void)
{
    bool ok = tsl_write8(0x00u, 0x03u); /* PON + AEN */
    ok = tsl_write8(0x01u, tsl_gain_code) && ok; /* 100 ms integration, auto gain */
    HAL_Delay(120);
    return ok;
}

static uint32_t tsl_gain_divisor(void)
{
    static const uint32_t gain_div[] = {1u, 25u, 428u, 9876u};
    uint8_t idx = tsl_gain_index;
    if (idx > 3u) idx = 3u;
    return gain_div[idx];
}

static void tsl2591_auto_gain(uint16_t ch0, uint16_t ch1)
{
    (void)ch0;
    (void)ch1;
    /* Fixed gain keeps intensity samples comparable across time. */
}

static bool tsl2591_read(uint16_t *ch0, uint16_t *ch1)
{
    bool ok = tsl_read16(0x14u, ch0) && tsl_read16(0x16u, ch1);
    if (ok) {
        uint16_t visible = (*ch0 > *ch1) ? (uint16_t)(*ch0 - *ch1) : 0u;
        uint32_t div = tsl_gain_divisor();
        last_tsl0_norm = ((uint32_t)(*ch0) * 4096u) / div;
        last_tsl_visible_norm = ((uint32_t)visible * 4096u) / div;
        tsl2591_auto_gain(*ch0, *ch1);
    }
    return ok;
}

static uint16_t spectrum_value(uint8_t ch)
{
    if (ch == SPEC_CLEAR_AVG) {
        return (uint16_t)(((uint32_t)last_as[4] + last_as[10] + last_as[16]) / 3u);
    }
    if (ch == SPEC_FD_AVG) {
        return (uint16_t)(((uint32_t)last_as[5] + last_as[11] + last_as[17]) / 3u);
    }
    if (ch < 18u) return last_as[ch];
    return 0u;
}

static void scan_i2c(char *buf, size_t n)
{
    size_t used = 0;
    used += (size_t)snprintf(buf + used, n - used, "I2C:");
    for (uint8_t addr = 1; addr < 127 && used < n - 8; addr++) {
        if (i2c_present(addr)) used += (size_t)snprintf(buf + used, n - used, " %02X", addr);
    }
}

static void i2c_select_mode(uint8_t mode)
{
    static const uint16_t scl_pins[] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_6, GPIO_PIN_7};
    static const uint16_t sda_pins[] = {GPIO_PIN_9, GPIO_PIN_8, GPIO_PIN_7, GPIO_PIN_6};

    if (mode > 3) mode = 0;
    i2c_mode = mode;
    bb_scl_pin = scl_pins[mode];
    bb_sda_pin = sda_pins[mode];
    i2c1_init();
}

static bool i2c_select_working_bus(void)
{
    for (uint8_t mode = 0; mode < 4; mode++) {
        i2c_select_mode(mode);
        if (i2c_present(TSL2591_ADDR) || i2c_present(AS7343_ADDR)) return true;
    }

    i2c_select_mode(0);
    return false;
}

static void draw_startup(void)
{
    LCD_Clear(UI_BG);
    POINT_COLOR = UI_TEXT;
    BACK_COLOR = UI_BG;
    /* text disabled: vendor font renderer faults in GCC build */
    /* text disabled: vendor font renderer faults in GCC build */
    uint16_t x = (lcddev.width > STARTUP_IMAGE_W) ? (uint16_t)((lcddev.width - STARTUP_IMAGE_W) / 2) : 0;
    LCD_Color_Fill(x, 100, (uint16_t)(x + STARTUP_IMAGE_W - 1), 100 + STARTUP_IMAGE_H - 1, (u16 *)startup_image_rgb565);
    /* text disabled: vendor font renderer faults in GCC build */
    HAL_Delay(800);
}

static void draw_frame(const char *scan)
{
    (void)scan;
    LCD_Clear(UI_BG);
    POINT_COLOR = UI_TEXT;
    BACK_COLOR = UI_BG;
    /* text disabled: vendor font renderer faults in GCC build */
    /* text disabled: vendor font renderer faults in GCC build */
    /* text disabled: vendor font renderer faults in GCC build */
    LCD_Fill(24, 104, (uint16_t)(lcddev.width - 24), 470, UI_PANEL);
    LCD_Fill(24, 488, (uint16_t)(lcddev.width - 24), 572, UI_PANEL);

    uint16_t panel_y = 130;
    uint16_t panel_h = 310;
    uint16_t gap = 36;
    uint16_t left_x = 46;
    uint16_t left_w = (uint16_t)((lcddev.width - 120 - gap) / 2);
    uint16_t right_x = (uint16_t)(left_x + left_w + gap);
    uint16_t right_w = (uint16_t)(lcddev.width - right_x - 46);

    LCD_Fill(left_x, panel_y, (uint16_t)(left_x + left_w), (uint16_t)(panel_y + panel_h), BLACK);
    LCD_Fill(right_x, panel_y, (uint16_t)(right_x + right_w), (uint16_t)(panel_y + panel_h), BLACK);
    POINT_COLOR = UI_GRID;
    LCD_DrawRectangle(left_x, panel_y, (uint16_t)(left_x + left_w), (uint16_t)(panel_y + panel_h));
    LCD_DrawRectangle(right_x, panel_y, (uint16_t)(right_x + right_w), (uint16_t)(panel_y + panel_h));
    LCD_DrawLine(right_x, (uint16_t)(panel_y + panel_h / 2), (uint16_t)(right_x + right_w), (uint16_t)(panel_y + panel_h / 2));
    spectrum_started = false;
    intensity_started = false;
    intensity_x = 0;
    tsl_visible_baseline = 0;
    tsl_full_baseline = 0;
    tsl_visible_span = 16;
    tsl_full_span = 16;
}

static void draw_status_boxes(bool as_ok, bool tsl_ok)
{
    uint16_t y = 26;
    uint16_t x = 26;

    LCD_Fill(x, y, (uint16_t)(x + 64), (uint16_t)(y + 42), as_ok ? UI_GREEN : UI_RED);
    LCD_Fill((uint16_t)(x + 84), y, (uint16_t)(x + 148), (uint16_t)(y + 42), tsl_ok ? UI_GREEN : UI_RED);

    for (uint8_t i = 0; i < 4; i++) {
        uint16_t bx = (uint16_t)(x + 190 + i * 48);
        uint16_t color = (i == i2c_mode) ? UI_CYAN : UI_GRID;
        LCD_Fill(bx, y, (uint16_t)(bx + 34), (uint16_t)(y + 42), color);
    }

    uint16_t sda_high = bb_read_sda() ? UI_GREEN : UI_RED;
    LCD_Fill((uint16_t)(x + 410), y, (uint16_t)(x + 474), (uint16_t)(y + 42), sda_high);
}

static const char *tiny_pattern(char c)
{
    switch (c) {
    case '0': return "111101101101111";
    case '1': return "010110010010111";
    case '2': return "111001111100111";
    case '3': return "111001111001111";
    case '4': return "101101111001001";
    case '5': return "111100111001111";
    case '6': return "111100111101111";
    case '7': return "111001010010010";
    case '8': return "111101111101111";
    case '9': return "111101111001111";
    case 'A': return "010101111101101";
    case 'C': return "111100100100111";
    case 'D': return "110101101101110";
    case 'F': return "111100110100100";
    case 'H': return "101101111101101";
    case 'I': return "111010010010111";
    case 'L': return "100100100100111";
    case 'N': return "101111111111101";
    case 'O': return "111101101101111";
    case 'R': return "110101110101101";
    case 'S': return "111100111001111";
    case 'T': return "111010010010010";
    case 'U': return "101101101101111";
    case 'V': return "101101101101010";
    case 'X': return "101101010101101";
    case '+': return "000010111010000";
    case '-': return "000000111000000";
    case ':': return "000010000010000";
    case '.': return "000000000000010";
    default: return "000000000000000";
    }
}

static void draw_tiny_char(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale)
{
    const char *p = tiny_pattern(c);
    if (scale == 0) scale = 1;
    for (uint8_t row = 0; row < 5; row++) {
        for (uint8_t col = 0; col < 3; col++) {
            if (p[row * 3u + col] == '1') {
                LCD_Fill((uint16_t)(x + col * scale),
                         (uint16_t)(y + row * scale),
                         (uint16_t)(x + col * scale + scale - 1u),
                         (uint16_t)(y + row * scale + scale - 1u),
                         color);
            }
        }
    }
}

static void draw_tiny_text(uint16_t x, uint16_t y, const char *s, uint16_t color, uint8_t scale)
{
    while (*s) {
        draw_tiny_char(x, y, *s, color, scale);
        x = (uint16_t)(x + 4u * scale);
        s++;
    }
}

static void draw_plot_percent_labels(uint16_t x0, uint16_t y0, uint16_t h)
{
    uint16_t lx = (x0 > 34u) ? (uint16_t)(x0 - 34u) : 0u;
    draw_tiny_text(lx, (y0 > 5u) ? (uint16_t)(y0 - 5u) : y0, "100", UI_GRID, 2);
    draw_tiny_text((uint16_t)(lx + 8u), (uint16_t)(y0 + h / 2u - 5u), "50", UI_GRID, 2);
    draw_tiny_text((uint16_t)(lx + 16u), (uint16_t)(y0 + h - 11u), "0", UI_GRID, 2);
}

static void draw_spectrum_axes(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h)
{
    POINT_COLOR = UI_GRID;
    LCD_DrawRectangle(x0, y0, (uint16_t)(x0 + w), (uint16_t)(y0 + h));
    LCD_DrawLine(x0, (uint16_t)(y0 + h / 2u), (uint16_t)(x0 + w), (uint16_t)(y0 + h / 2u));
    LCD_DrawLine(x0, (uint16_t)(y0 + h / 4u), (uint16_t)(x0 + w), (uint16_t)(y0 + h / 4u));
    LCD_DrawLine(x0, (uint16_t)(y0 + (3u * h) / 4u), (uint16_t)(x0 + w), (uint16_t)(y0 + (3u * h) / 4u));
    draw_plot_percent_labels(x0, y0, h);

    static const uint8_t tick_idx[] = {0, 5, 9, 10};
    static const char *tick_lab[] = {"405", "550", "690", "745"};
    for (uint8_t i = 0; i < sizeof(tick_idx); i++) {
        uint16_t tx = (uint16_t)(x0 + 16u + ((uint32_t)tick_idx[i] * (w - 32u)) / (SPEC_MAIN_COUNT - 1u));
        POINT_COLOR = UI_GRID;
        LCD_DrawLine(tx, (uint16_t)(y0 + h - 5u), tx, (uint16_t)(y0 + h + 5u));
        draw_tiny_text((tx > 12u) ? (uint16_t)(tx - 12u) : tx, (uint16_t)(y0 + h + 10u), tick_lab[i], UI_GRID, 2);
    }
}

static void draw_intensity_axes(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, bool fixed)
{
    POINT_COLOR = UI_GRID;
    LCD_DrawRectangle(x0, y0, (uint16_t)(x0 + w), (uint16_t)(y0 + h));
    LCD_DrawLine(x0, (uint16_t)(y0 + h / 2u), (uint16_t)(x0 + w), (uint16_t)(y0 + h / 2u));
    LCD_DrawLine(x0, (uint16_t)(y0 + h / 4u), (uint16_t)(x0 + w), (uint16_t)(y0 + h / 4u));
    LCD_DrawLine(x0, (uint16_t)(y0 + (3u * h) / 4u), (uint16_t)(x0 + w), (uint16_t)(y0 + (3u * h) / 4u));
    if (fixed) {
        draw_plot_percent_labels(x0, y0, h);
    } else {
        uint16_t lx = (x0 > 22u) ? (uint16_t)(x0 - 22u) : 0u;
        draw_tiny_text(lx, (y0 > 5u) ? (uint16_t)(y0 - 5u) : y0, "+", UI_GRID, 2);
        draw_tiny_text(lx, (uint16_t)(y0 + h / 2u - 5u), "0", UI_GRID, 2);
        draw_tiny_text(lx, (uint16_t)(y0 + h - 11u), "-", UI_GRID, 2);
    }
}

static uint16_t trace_y(uint32_t v, uint32_t maxv, uint16_t top, uint16_t h)
{
    if (maxv == 0) maxv = 1;
    if (v > maxv) v = maxv;
    return (uint16_t)(top + h - 1 - ((v * (h - 2)) / maxv));
}

static uint16_t centered_trace_y(uint32_t v, uint32_t *baseline, uint32_t *span, uint16_t top, uint16_t h)
{
    if (*baseline == 0 && v > 0) *baseline = v;

    int32_t diff = (int32_t)v - (int32_t)(*baseline);
    uint32_t adiff = (diff < 0) ? (uint32_t)(-diff) : (uint32_t)diff;
    uint32_t target_span = adiff * 4u + 8u;
    if (target_span < 16u) target_span = 16u;

    *baseline = ((*baseline * 63u) + v) / 64u;
    if (target_span > *span) *span = target_span;
    else *span = ((*span * 127u) + target_span) / 128u;
    if (*span < 16u) *span = 16u;

    int32_t center = (int32_t)top + (int32_t)h / 2;
    int32_t half = ((int32_t)h / 2) - 8;
    int32_t y = center - ((diff * half) / (int32_t)(*span));
    int32_t min_y = (int32_t)top + 2;
    int32_t max_y = (int32_t)top + (int32_t)h - 2;
    if (y < min_y) y = min_y;
    if (y > max_y) y = max_y;
    return (uint16_t)y;
}

static void draw_aux_spectrum_bars(uint16_t x0, uint16_t y0, uint16_t w, bool as_ok)
{
    uint16_t bx = (uint16_t)(x0 + w - 92u);
    uint16_t by = (uint16_t)(y0 + 14u);
    uint16_t bw = 78u;
    uint16_t bh = 88u;
    uint32_t vals[3];
    vals[0] = spectrum_value(3u);              /* NIR 855 nm */
    vals[1] = spectrum_value(SPEC_CLEAR_AVG);  /* clear / VIS average */
    vals[2] = spectrum_value(SPEC_FD_AVG);     /* flicker-detect average */

    if (scale_fixed_mode) {
        as_aux_bar_max = AS_FIXED_AUX_MAX;
    } else {
        uint32_t peak = 1;
        for (uint8_t i = 0; i < 3; i++) if (vals[i] > peak) peak = vals[i];
        if (peak > as_aux_bar_max) as_aux_bar_max = peak;
        else as_aux_bar_max = ((as_aux_bar_max * 31u) + peak + 1u) / 32u;
        if (as_aux_bar_max < 100u) as_aux_bar_max = 100u;
    }

    LCD_Fill(bx, by, (uint16_t)(bx + bw), (uint16_t)(by + bh), BLACK);
    POINT_COLOR = UI_GRID;
    LCD_DrawRectangle(bx, by, (uint16_t)(bx + bw), (uint16_t)(by + bh));
    draw_tiny_text((uint16_t)(bx + 6u), (uint16_t)(by + 5u), "N C F", UI_GRID, 1);

    for (uint8_t i = 0; i < 3; i++) {
        uint16_t bar_x = (uint16_t)(bx + 12u + i * 22u);
        uint16_t bar_top = (uint16_t)(by + 20u);
        uint16_t bar_bot = (uint16_t)(by + bh - 13u);
        uint16_t bar_h = (uint16_t)(bar_bot - bar_top);
        uint16_t fill_h = (uint16_t)((vals[i] * bar_h) / as_aux_bar_max);
        if (fill_h > bar_h) fill_h = bar_h;
        uint16_t color = (i == 0) ? UI_RED : ((i == 1) ? UI_CYAN : UI_YELLOW);
        LCD_Fill(bar_x, bar_top, (uint16_t)(bar_x + 10u), bar_bot, UI_PANEL);
        if (as_ok && fill_h > 0) {
            LCD_Fill(bar_x, (uint16_t)(bar_bot - fill_h), (uint16_t)(bar_x + 10u), bar_bot, color);
        }
    }
}

static void draw_tsl_channel_bars(bool tsl_ok)
{
    uint16_t w = 230u;
    uint16_t h = 76u;
    uint16_t x = (lcddev.width > (w + 28u)) ? (uint16_t)(lcddev.width - w - 28u) : 520u;
    uint16_t y = 18u;
    uint32_t vals[2] = {last_tsl0, last_tsl1};
    if (scale_fixed_mode) {
        tsl_bar_max = TSL_FIXED_RAW_MAX;
    } else {
        uint32_t peak = (vals[0] > vals[1]) ? vals[0] : vals[1];
        if (peak < 16u) peak = 16u;
        if (peak > tsl_bar_max) tsl_bar_max = peak;
        else tsl_bar_max = ((tsl_bar_max * 31u) + peak + 1u) / 32u;
        if (tsl_bar_max < 64u) tsl_bar_max = 64u;
    }

    LCD_Fill(x, y, (uint16_t)(x + w), (uint16_t)(y + h), BLACK);
    POINT_COLOR = UI_GRID;
    LCD_DrawRectangle(x, y, (uint16_t)(x + w), (uint16_t)(y + h));
    draw_tiny_text((uint16_t)(x + 8u), (uint16_t)(y + 6u), scale_fixed_mode ? "FIX C0 C1" : "AUTO C0 C1", UI_GRID, 2);

    for (uint8_t i = 0; i < 2; i++) {
        uint16_t bar_x = (uint16_t)(x + 92u + i * 58u);
        uint16_t bar_top = (uint16_t)(y + 22u);
        uint16_t bar_bot = (uint16_t)(y + h - 11u);
        uint16_t bar_h = (uint16_t)(bar_bot - bar_top);
        uint16_t fill_h = (uint16_t)((vals[i] * bar_h) / tsl_bar_max);
        if (fill_h > bar_h) fill_h = bar_h;
        uint16_t color = (i == 0) ? UI_ORANGE : UI_RED;
        LCD_Fill(bar_x, bar_top, (uint16_t)(bar_x + 30u), bar_bot, UI_PANEL);
        if (tsl_ok && fill_h > 0) {
            LCD_Fill(bar_x, (uint16_t)(bar_bot - fill_h), (uint16_t)(bar_x + 30u), bar_bot, color);
        }
        draw_tiny_text((uint16_t)(bar_x + 10u), (uint16_t)(bar_bot + 3u), (i == 0) ? "0" : "1", UI_GRID, 1);
    }
}

static void draw_live(uint32_t t_ms, bool as_ok, bool tsl_ok)
{
    uint16_t plot_y = 130;
    uint16_t plot_h = 310;
    uint16_t gap = 36;
    uint16_t left_x = 46;
    uint16_t left_w = (uint16_t)((lcddev.width - 120 - gap) / 2);
    uint16_t right_x = (uint16_t)(left_x + left_w + gap);
    uint16_t right_w = (uint16_t)(lcddev.width - right_x - 46);
    uint8_t n_main = SPEC_MAIN_COUNT;
    (void)t_ms;

    draw_status_boxes(as_ok, tsl_ok);
    draw_tsl_channel_bars(tsl_ok);

    uint32_t visible = last_tsl_visible_norm;
    uint32_t full_norm = last_tsl0_norm;
    uint32_t diag = (!as_ok && !tsl_ok) ? ((seq & 0x20u) ? 18u : 65u) : 0u;

    uint32_t visible_peak = 1;
    for (uint8_t i = 0; i < n_main; i++) {
        uint16_t v = spectrum_value(spec_ch[i]);
        if (v > visible_peak) visible_peak = v;
    }
    if (scale_fixed_mode) {
        as_trace_max = AS_FIXED_VISIBLE_MAX;
    } else {
        if (visible_peak > as_trace_max) as_trace_max = visible_peak;
        else if (as_trace_max > 100) as_trace_max -= (as_trace_max / 512u) + 1u;
    }

    uint16_t spec_y[SPEC_MAIN_COUNT];
    uint32_t spec_max = (as_trace_max < 100u) ? 100u : as_trace_max;

    for (uint8_t i = 0; i < n_main; i++) {
        uint16_t x0 = (uint16_t)(left_x + 16 + ((uint32_t)i * (left_w - 32)) / (n_main - 1u));
        spec_y[i] = trace_y(spectrum_value(spec_ch[i]), spec_max, plot_y, plot_h);
        if (spectrum_started) {
            uint16_t px = (uint16_t)(left_x + 16 + ((uint32_t)i * (left_w - 32)) / (n_main - 1u));
            LCD_Fill((uint16_t)(px - 3u), (uint16_t)(prev_spec_y[i] - 3u), (uint16_t)(px + 3u), (uint16_t)(prev_spec_y[i] + 3u), BLACK);
        }
        if (spectrum_started && i > 0) {
            uint16_t px0 = (uint16_t)(left_x + 16 + ((uint32_t)(i - 1) * (left_w - 32)) / (n_main - 1u));
            POINT_COLOR = BLACK;
            LCD_DrawLine(px0, prev_spec_y[i - 1], x0, prev_spec_y[i]);
        }
    }
    draw_spectrum_axes(left_x, plot_y, left_w, plot_h);
    draw_tiny_text((uint16_t)(left_x + 12u), (uint16_t)(plot_y + 8u), "AS7343", as_ok ? UI_CYAN : UI_RED, 2);
    draw_aux_spectrum_bars(left_x, plot_y, left_w, as_ok);
    for (uint8_t i = 1; i < n_main; i++) {
        uint16_t x0 = (uint16_t)(left_x + 16 + ((uint32_t)(i - 1) * (left_w - 32)) / (n_main - 1u));
        uint16_t x1 = (uint16_t)(left_x + 16 + ((uint32_t)i * (left_w - 32)) / (n_main - 1u));
        POINT_COLOR = as_ok ? UI_CYAN : UI_RED;
        LCD_DrawLine(x0, spec_y[i - 1], x1, spec_y[i]);
    }
    for (uint8_t i = 0; i < n_main; i++) {
        uint16_t x0 = (uint16_t)(left_x + 16 + ((uint32_t)i * (left_w - 32)) / (n_main - 1u));
        LCD_Fill((uint16_t)(x0 - 2u), (uint16_t)(spec_y[i] - 2u), (uint16_t)(x0 + 2u), (uint16_t)(spec_y[i] + 2u), as_ok ? UI_YELLOW : UI_RED);
    }
    memcpy(prev_spec_y, spec_y, sizeof(spec_y));
    spectrum_started = true;

    uint16_t x = (uint16_t)(right_x + intensity_x);
    uint16_t erase_x1 = (uint16_t)(x + 3);
    if (erase_x1 > (uint16_t)(right_x + right_w)) erase_x1 = (uint16_t)(right_x + right_w);
    LCD_Fill(x, (uint16_t)(plot_y + 1), erase_x1, (uint16_t)(plot_y + plot_h - 1), BLACK);
    draw_intensity_axes(right_x, plot_y, right_w, plot_h, scale_fixed_mode);
    draw_tiny_text((uint16_t)(right_x + 12u), (uint16_t)(plot_y + 8u), "TSL2591", tsl_ok ? UI_GREEN : UI_RED, 2);

    uint16_t raw_visible = (last_tsl0 > last_tsl1) ? (uint16_t)(last_tsl0 - last_tsl1) : 0u;
    uint16_t tsl_y = scale_fixed_mode ?
        trace_y(raw_visible, TSL_FIXED_RAW_MAX, plot_y, plot_h) :
        centered_trace_y(visible, &tsl_visible_baseline, &tsl_visible_span, plot_y, plot_h);
    uint16_t full_y = scale_fixed_mode ?
        trace_y(last_tsl0, TSL_FIXED_RAW_MAX, plot_y, plot_h) :
        centered_trace_y(full_norm, &tsl_full_baseline, &tsl_full_span, plot_y, plot_h);
    uint16_t diag_y = trace_y(diag, 100u, plot_y, plot_h);

    if (!intensity_started || intensity_x == 0) {
        prev_tsl_y = tsl_y;
        prev_full_y = full_y;
        prev_diag_y = diag_y;
        intensity_started = true;
    }

    uint16_t prev_x = (intensity_x >= 3) ? (uint16_t)(x - 3) : x;
    POINT_COLOR = tsl_ok ? UI_ORANGE : UI_RED;
    LCD_DrawLine(prev_x, prev_full_y, x, full_y);
    POINT_COLOR = tsl_ok ? UI_GREEN : UI_RED;
    LCD_DrawLine(prev_x, prev_tsl_y, x, tsl_y);
    if (!as_ok && !tsl_ok) {
        POINT_COLOR = UI_RED;
        LCD_DrawLine(prev_x, prev_diag_y, x, diag_y);
    }

    prev_tsl_y = tsl_y;
    prev_full_y = full_y;
    prev_diag_y = diag_y;
    intensity_x = (uint16_t)(intensity_x + 3);
    if (intensity_x >= right_w) intensity_x = 0;
}

static void capture_log_sample(uint32_t t_ms, bool as_ok, bool tsl_ok)
{
    uint32_t idx = capture_log_write % CAPTURE_LOG_CAPACITY;
    CaptureRecord *r = (CaptureRecord *)&capture_log[idx];
    r->t_ms = t_ms;
    r->seq = seq;
    r->as_samples = as_sample_count;
    r->tsl_samples = tsl_sample_count;
    r->tsl_ch0 = last_tsl0;
    r->tsl_ch1 = last_tsl1;
    r->tsl_visible = (last_tsl0 > last_tsl1) ? (uint16_t)(last_tsl0 - last_tsl1) : 0u;
    r->tsl_gain = tsl_gain_code;
    r->ok_flags = (uint16_t)((as_ok ? 1u : 0u) | (tsl_ok ? 2u : 0u) | (scale_fixed_mode ? 4u : 0u));
    r->status2 = last_status2;
    r->as_gain = as_gain_code;
    r->reserved = 0;
    for (uint8_t i = 0; i < 18u; i++) r->ch[i] = last_as[i];
    capture_log_write++;
}

static void print_csv_header(void)
{
    printf("# STM32H743 sensor head\r\n");
    printf("# AS7343 addr=0x39 TSL2591 addr=0x29 I2C1 PB8/PB9\r\n");
    printf("# serial command: s toggles AUTO_DYNAMIC / FIXED_RAW display scale\r\n");
    printf("# sensor gain is fixed; firmware auto-gain is disabled\r\n");
    printf("t_ms,seq,ok_tsl,tsl_ch0,tsl_ch1,tsl_visible,tsl_gain,tsl_full_norm,tsl_visible_norm,tsl_samples,ok_as7343,status2,as_gain,as_samples,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8,ch9,ch10,ch11,ch12,ch13,ch14,ch15,ch16,ch17,sum_display\r\n");
}

static bool poll_serial_commands(void)
{
    bool changed = false;
    uint8_t c = 0;
    while (HAL_UART_Receive(&UART1_Handler, &c, 1, 0) == HAL_OK) {
        if (c == 's' || c == 'S') {
            scale_fixed_mode = !scale_fixed_mode;
            changed = true;
            printf("# scale_mode=%s\r\n", scale_fixed_mode ? "FIXED_RAW" : "AUTO_DYNAMIC");
        }
    }
    return changed;
}

static void print_csv(uint32_t t_ms, bool as_ok, bool tsl_ok)
{
    uint16_t visible = (last_tsl0 > last_tsl1) ? (uint16_t)(last_tsl0 - last_tsl1) : 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < sizeof(spec_ch); i++) sum += spectrum_value(spec_ch[i]);
    printf("%lu,%lu,%u,%u,%u,%u,%u,%lu,%lu,%lu,%u,%u,%u,%lu",
           (unsigned long)t_ms,
           (unsigned long)seq,
           tsl_ok ? 1 : 0,
           last_tsl0,
           last_tsl1,
           visible,
           tsl_gain_code,
           (unsigned long)last_tsl0_norm,
           (unsigned long)last_tsl_visible_norm,
           (unsigned long)tsl_sample_count,
           as_ok ? 1 : 0,
           last_status2,
           as_gain_code,
           (unsigned long)as_sample_count);
    for (uint8_t i = 0; i < 18; i++) printf(",%u", last_as[i]);
    printf(",%lu\r\n", (unsigned long)sum);
}

int main(void)
{
    char scan[128];
    uint32_t last_probe = 0;
    uint32_t last_as_read = 0;
    uint32_t last_tsl_read = 0;
    uint32_t last_draw = 0;
    bool as_ok_live = false;
    bool tsl_ok_live = false;

    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4); /* 400 MHz */
    delay_init(400);
    uart_init(115200);
    capture_log_init_metadata();
    SDRAM_Init();
    LCD_Init();
    LCD_Display_Dir(1);

    draw_startup();
    i2c_select_working_bus();
    scan_i2c(scan, sizeof(scan));

    ok_tsl = i2c_present(TSL2591_ADDR) && tsl2591_configure();
    ok_as7343 = i2c_present(AS7343_ADDR) && as7343_configure();

    draw_frame(scan);
    print_csv_header();
    printf("# %s mode=%u\\r\\n", scan, i2c_mode);
    printf("# tsl2591=%s as7343=%s\r\n", ok_tsl ? "yes" : "no", ok_as7343 ? "yes" : "no");

    while (1) {
        uint32_t now = HAL_GetTick();
        if (poll_serial_commands()) {
            draw_frame(scan);
        }

        if ((now - last_probe) > 1000u && (!ok_tsl || !ok_as7343)) {
            i2c_select_working_bus();
            scan_i2c(scan, sizeof(scan));
            if (!ok_tsl) ok_tsl = i2c_present(TSL2591_ADDR) && tsl2591_configure();
            if (!ok_as7343) ok_as7343 = i2c_present(AS7343_ADDR) && as7343_configure();
            last_probe = now;
        }

        if (ok_as7343 && (now - last_as_read) >= AS_PERIOD_MS) {
            as_ok_live = as7343_read18(last_as, &last_status2);
            if (as_ok_live) as_sample_count++;
            last_as_read = now;
        }

        if (ok_tsl && (now - last_tsl_read) >= TSL_PERIOD_MS) {
            tsl_ok_live = tsl2591_read(&last_tsl0, &last_tsl1);
            if (tsl_ok_live) tsl_sample_count++;
            last_tsl_read = now;
        }

        if ((now - last_draw) >= DRAW_PERIOD_MS) {
            seq++;
            capture_log_sample(now, as_ok_live, tsl_ok_live);
            print_csv(now, as_ok_live, tsl_ok_live);
            draw_live(now, as_ok_live, tsl_ok_live);
            last_draw = now;
        }

        HAL_Delay(1);
    }
}



