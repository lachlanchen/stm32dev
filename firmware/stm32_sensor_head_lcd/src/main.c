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

#define UI_BG                0x0841u
#define UI_PANEL             0x18E3u
#define UI_GRID              0x39E7u
#define UI_TEXT              0xFFFFu
#define UI_GREEN             0x07E0u
#define UI_ORANGE            0xFD20u
#define UI_CYAN              0x07FFu
#define UI_RED               0xF800u

I2C_HandleTypeDef hi2c1;

static bool ok_tsl = false;
static bool ok_as7343 = false;
static uint16_t last_as[18];
static uint16_t last_tsl0 = 0;
static uint16_t last_tsl1 = 0;
static uint8_t last_status2 = 0;
static uint32_t seq = 0;

static const uint8_t spec_ch[] = {12, 6, 0, 7, 8, 15, 1, 2, 9, 13, 14, 3};
static const uint16_t spec_nm[] = {405, 425, 450, 475, 515, 550, 555, 600, 640, 690, 745, 855};
static const uint16_t spec_color[] = {0x801F,0x401F,0x03FF,0x04FF,0x07E0,0xAFE0,0xFFE0,0xFDE0,0xFA20,0xF820,0xF810,0x780F};

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
    HAL_Delay(10);
    if (!i2c_write8(AS7343_ADDR, AS_REG_ATIME, 29u)) return false;
    if (!i2c_write16_le(AS7343_ADDR, AS_REG_ASTEP_L, 599u)) return false;
    if (!i2c_write8(AS7343_ADDR, AS_REG_CFG1, 5u)) return false; /* 16x gain */
    uint8_t cfg20 = 0;
    if (!i2c_read8(AS7343_ADDR, AS_REG_CFG20, &cfg20)) return false;
    cfg20 &= (uint8_t)~0x60u;
    cfg20 |= 0x60u; /* auto SMUX, 18-channel mode */
    if (!i2c_write8(AS7343_ADDR, AS_REG_CFG20, cfg20)) return false;
    if (!i2c_write8(AS7343_ADDR, AS_REG_ENABLE, 0x03u)) return false;
    HAL_Delay(180);
    return true;
}

static bool as7343_read18(uint16_t *channels, uint8_t *status2)
{
    uint8_t raw[36];
    if (!as_set_bank(false)) return false;
    if (!i2c_write8(AS7343_ADDR, AS_REG_ENABLE, 0x01u)) return false;
    HAL_Delay(2);
    if (!i2c_write8(AS7343_ADDR, AS_REG_ENABLE, 0x03u)) return false;
    HAL_Delay(180);
    i2c_read8(AS7343_ADDR, AS_REG_STATUS2, status2);
    if (!i2c_read_bytes(AS7343_ADDR, AS_REG_DATA0, raw, sizeof(raw))) return false;
    for (uint8_t i = 0; i < 18; i++) channels[i] = ((uint16_t)raw[2u * i + 1u] << 8) | raw[2u * i];
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
    ok = tsl_write8(0x01u, 0x00u) && ok; /* 100 ms integration, low gain */
    HAL_Delay(120);
    return ok;
}

static bool tsl2591_read(uint16_t *ch0, uint16_t *ch1)
{
    return tsl_read16(0x14u, ch0) && tsl_read16(0x16u, ch1);
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

static uint16_t scale_bar(uint32_t v, uint32_t maxv, uint16_t h)
{
    if (maxv == 0) return 0;
    uint32_t y = (v * h) / maxv;
    if (y > h) y = h;
    return (uint16_t)y;
}

static void draw_live(uint32_t t_ms, bool as_ok, bool tsl_ok)
{
    char line[160];
    uint16_t plot_x = 60;
    uint16_t plot_y = 130;
    uint16_t plot_w = (lcddev.width > 180) ? (uint16_t)(lcddev.width - 140) : 840;
    uint16_t plot_h = 290;
    uint8_t n = (uint8_t)(sizeof(spec_ch) / sizeof(spec_ch[0]));
    uint16_t bar_w = (uint16_t)(plot_w / (n * 2));
    if (bar_w < 12) bar_w = 12;

    (void)t_ms;
    LCD_Fill(28, 108, (uint16_t)(lcddev.width - 28), 466, UI_PANEL);
    draw_status_boxes(as_ok, tsl_ok);
    LCD_Fill(plot_x, plot_y, (uint16_t)(plot_x + plot_w), (uint16_t)(plot_y + plot_h), BLACK);
    POINT_COLOR = UI_GRID;
    LCD_DrawRectangle(plot_x, plot_y, (uint16_t)(plot_x + plot_w), (uint16_t)(plot_y + plot_h));
    for (uint8_t g = 1; g < 4; g++) {
        uint16_t yy = (uint16_t)(plot_y + (plot_h * g) / 4);
        LCD_DrawLine(plot_x, yy, (uint16_t)(plot_x + plot_w), yy);
    }

    uint32_t maxv = 1;
    for (uint8_t i = 0; i < n; i++) {
        uint16_t v = last_as[spec_ch[i]];
        if (v > maxv) maxv = v;
    }
    for (uint8_t i = 0; i < n; i++) {
        uint16_t v = last_as[spec_ch[i]];
        uint16_t bh = scale_bar(v, maxv, (uint16_t)(plot_h - 12));
        uint16_t x0 = (uint16_t)(plot_x + 18 + i * (plot_w - 36) / n);
        uint16_t y0 = (uint16_t)(plot_y + plot_h - bh);
        LCD_Fill(x0, y0, (uint16_t)(x0 + bar_w), (uint16_t)(plot_y + plot_h - 1), spec_color[i]);
        POINT_COLOR = UI_TEXT;
        BACK_COLOR = UI_PANEL;
        snprintf(line, sizeof(line), "%u", spec_nm[i]);
        /* text disabled: vendor font renderer faults in GCC build */
    }

    uint32_t as_sum = 0;
    for (uint8_t i = 0; i < n; i++) as_sum += last_as[spec_ch[i]];
    uint16_t visible = (last_tsl0 > last_tsl1) ? (uint16_t)(last_tsl0 - last_tsl1) : 0;

    POINT_COLOR = UI_TEXT;
    BACK_COLOR = UI_PANEL;
    snprintf(line, sizeof(line), "t=%lu ms seq=%lu AS7343=%s TSL2591=%s max=%lu sum=%lu status2=0x%02X", (unsigned long)t_ms, (unsigned long)seq, as_ok ? "OK" : "MISS", tsl_ok ? "OK" : "MISS", (unsigned long)maxv, (unsigned long)as_sum, last_status2);
    /* text disabled: vendor font renderer faults in GCC build */
    /* text disabled: vendor font renderer faults in GCC build */

    LCD_Fill(28, 492, (uint16_t)(lcddev.width - 28), 568, UI_PANEL);
    snprintf(line, sizeof(line), "TSL2591 full=%u ir=%u visible=%u", last_tsl0, last_tsl1, visible);
    /* text disabled: vendor font renderer faults in GCC build */
    uint16_t meter_x = 44;
    uint16_t meter_y = 536;
    uint16_t meter_w = (uint16_t)(lcddev.width - 120);
    uint16_t meter_h = 18;
    LCD_Fill(meter_x, meter_y, (uint16_t)(meter_x + meter_w), (uint16_t)(meter_y + meter_h), BLACK);
    uint16_t fill = scale_bar(visible, 65535u, meter_w);
    LCD_Fill(meter_x, meter_y, (uint16_t)(meter_x + fill), (uint16_t)(meter_y + meter_h), UI_GREEN);
}

static void print_csv_header(void)
{
    printf("# STM32H743 sensor head\r\n");
    printf("# AS7343 addr=0x39 TSL2591 addr=0x29 I2C1 PB8/PB9\r\n");
    printf("t_ms,seq,ok_tsl,tsl_ch0,tsl_ch1,tsl_visible,ok_as7343,status2,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8,ch9,ch10,ch11,ch12,ch13,ch14,ch15,ch16,ch17,sum_visible\r\n");
}

static void print_csv(uint32_t t_ms, bool as_ok, bool tsl_ok)
{
    uint16_t visible = (last_tsl0 > last_tsl1) ? (uint16_t)(last_tsl0 - last_tsl1) : 0;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < sizeof(spec_ch); i++) sum += last_as[spec_ch[i]];
    printf("%lu,%lu,%u,%u,%u,%u,%u,%u", (unsigned long)t_ms, (unsigned long)seq, tsl_ok ? 1 : 0, last_tsl0, last_tsl1, visible, as_ok ? 1 : 0, last_status2);
    for (uint8_t i = 0; i < 18; i++) printf(",%u", last_as[i]);
    printf(",%lu\r\n", (unsigned long)sum);
}

int main(void)
{
    char scan[128];
    uint32_t last_probe = 0;

    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4); /* 400 MHz */
    delay_init(400);
    uart_init(115200);
    SDRAM_Init();
    LCD_Init();

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
        if ((now - last_probe) > 1000u && (!ok_tsl || !ok_as7343)) {
            i2c_select_working_bus();
            scan_i2c(scan, sizeof(scan));
            if (!ok_tsl) ok_tsl = i2c_present(TSL2591_ADDR) && tsl2591_configure();
            if (!ok_as7343) ok_as7343 = i2c_present(AS7343_ADDR) && as7343_configure();
            last_probe = now;
        }
        bool tsl_ok = ok_tsl && tsl2591_read(&last_tsl0, &last_tsl1);
        bool as_ok = ok_as7343 && as7343_read18(last_as, &last_status2);
        seq++;
        print_csv(now, as_ok, tsl_ok);
        draw_live(now, as_ok, tsl_ok);
        HAL_Delay(20);
    }
}



