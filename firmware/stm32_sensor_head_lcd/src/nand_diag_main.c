#include "main.h"
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "sdram.h"
#include "lcd.h"
#include "nand_diag_i18n.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Standalone, read-only solder diagnostic for Samsung K9F2G08U0C-SIB0.
 *
 * Safety contract:
 *   - no page-program command (0x80/0x10)
 *   - no block-erase command (0x60/0xD0)
 *   - only RESET (0xFF), READ STATUS (0x70), and READ ID (0x90)
 *
 * The normal sensor-head entry point remains src/main.c. This file is built
 * only by `make nand-diag` / `make flash-nand-diag`.
 */

#define NAND_DATA_ADDRESS       0x80000000UL
#define NAND_COMMAND_ADDRESS    0x80010000UL
#define NAND_LATCH_ADDRESS      0x80020000UL

#define DIAG_NAND_CMD_RESET          0xFFu
#define DIAG_NAND_CMD_READ_STATUS    0x70u
#define DIAG_NAND_CMD_READ_ID        0x90u

#define NAND_STATUS_FAIL        0x01u
#define NAND_STATUS_READY       0x40u
#define NAND_STATUS_WP_HIGH     0x80u

#define CHECKS_PER_TIMING       32u
#define TIMING_PROFILE_COUNT    3u
#define TOTAL_ID_CHECKS         (CHECKS_PER_TIMING * TIMING_PROFILE_COUNT)

#define FAIL_FMC                (1u << 0)
#define FAIL_RB_IDLE            (1u << 1)
#define FAIL_RESET              (1u << 2)
#define FAIL_STATUS             (1u << 3)
#define FAIL_WP                 (1u << 4)
#define FAIL_ID                 (1u << 5)
#define FAIL_STABILITY          (1u << 6)
#define FAIL_BUS_STUCK          (1u << 7)

#define UI_BG                   0x0841u
#define UI_PANEL                0x18E3u
#define UI_GREEN                0x3666u
#define UI_RED                  0xD986u
#define UI_YELLOW               0xFD20u
#define UI_CYAN                 0x4E9Fu
#define UI_TEXT                 0xFFFFu
#define UI_MUTED                0xA514u

static const uint8_t supported_id_15[5] = {0xECu, 0xDAu, 0x10u, 0x15u, 0x44u};
static const uint8_t supported_id_95[5] = {0xECu, 0xDAu, 0x10u, 0x95u, 0x44u};
static const uint32_t timing_profiles[TIMING_PROFILE_COUNT] = {
    0x0F0F0F0Fu,
    0x0A0A0A0Au,
    0x08080808u,
};

typedef struct {
    uint8_t id[5];
    uint8_t status;
    uint8_t rb_idle;
    uint8_t rb_busy_seen;
    uint8_t reset_ready;
    uint8_t expected_reads;
    uint8_t consistent_reads;
    uint32_t fail_mask;
} nand_result_t;

/* Stable symbols for live ST-Link inspection even if the LCD is unavailable. */
volatile uint32_t nand_diag_magic = 0x4E414E44u;
volatile uint32_t nand_diag_result = 0u;
volatile uint32_t nand_diag_fail_mask = 0xFFFFFFFFu;
volatile uint32_t nand_diag_id_0_3 = 0u;
volatile uint32_t nand_diag_id_4_status = 0u;
volatile uint32_t nand_diag_rb_flags = 0u;
volatile uint32_t nand_diag_expected_reads = 0u;
volatile uint32_t nand_diag_consistent_reads = 0u;
volatile uint32_t nand_diag_cycles = 0u;
volatile uint32_t nand_diag_pass_cycles = 0u;
volatile uint32_t nand_diag_fail_cycles = 0u;
volatile uint32_t nand_diag_language = NAND_LANG_ZH_CN;

static nand_language_t ui_language = NAND_LANG_ZH_CN;

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static inline void nand_write_command(uint8_t value)
{
    *(volatile uint8_t *)NAND_COMMAND_ADDRESS = value;
    __DSB();
}

static inline void nand_write_address(uint8_t value)
{
    *(volatile uint8_t *)NAND_LATCH_ADDRESS = value;
    __DSB();
}

static inline uint8_t nand_read_data(void)
{
    uint8_t value = *(volatile uint8_t *)NAND_DATA_ADDRESS;
    __DSB();
    return value;
}

static bool rb_ready(void)
{
    return HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_6) == GPIO_PIN_SET;
}

static void lamps_force_off(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
}

static void nand_mpu_config(void)
{
    MPU_Region_InitTypeDef region = {0};

    HAL_MPU_Disable();
    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER3;
    region.BaseAddress = NAND_DATA_ADDRESS;
    region.Size = MPU_REGION_SIZE_256MB;
    region.SubRegionDisable = 0x00u;
    region.TypeExtField = MPU_TEX_LEVEL0;
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    region.IsShareable = MPU_ACCESS_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&region);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
    __DSB();
    __ISB();
}

static bool nand_bus_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* R/B# is sampled as an ordinary input so a floating/busy pin is visible. */
    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FMC;

    /* PD0/1=data, PD4=RE#, PD5=WE#, PD11=ALE, PD12=CLE, PD14/15=data. */
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
               GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* PE7..PE10 are the remaining four data bits. */
    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* PG9 is FMC NCE3. */
    gpio.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOG, &gpio);

    nand_mpu_config();

    /* Conservative asynchronous NAND timing on FMC Bank 3, 8-bit, ECC off. */
    FMC_Bank3->PCR = 0x00035400u;
    FMC_Bank3->PMEM = timing_profiles[0];
    FMC_Bank3->PATT = timing_profiles[0];
    FMC_Bank3->PCR = 0x00035404u;
    __DSB();

    return (FMC_Bank3->PCR & 0x04u) != 0u;
}

static void nand_set_timing(uint32_t timing)
{
    uint32_t pcr = FMC_Bank3->PCR;
    FMC_Bank3->PCR = pcr & ~0x04u;
    __DSB();
    FMC_Bank3->PMEM = timing;
    FMC_Bank3->PATT = timing;
    FMC_Bank3->PCR = pcr | 0x04u;
    __DSB();
}

static uint8_t nand_read_status(void)
{
    nand_write_command(DIAG_NAND_CMD_READ_STATUS);
    delay_us(2);
    return nand_read_data();
}

static bool nand_reset(bool *busy_seen)
{
    bool saw_busy = false;

    nand_write_command(DIAG_NAND_CMD_RESET);
    for (uint32_t us = 0; us < 5000u; ++us) {
        if (!rb_ready()) saw_busy = true;
        if (saw_busy && rb_ready()) break;
        delay_us(1);
    }

    for (uint32_t us = 0; us < 10000u; ++us) {
        if (rb_ready()) {
            *busy_seen = saw_busy;
            return true;
        }
        delay_us(1);
    }

    *busy_seen = saw_busy;
    return false;
}

static void nand_read_id(uint8_t id[5])
{
    nand_write_command(DIAG_NAND_CMD_READ_ID);
    nand_write_address(0x00u);
    delay_us(2);
    for (uint8_t i = 0; i < 5u; ++i) id[i] = nand_read_data();
}

static bool bytes_equal(const uint8_t a[5], const uint8_t b[5])
{
    for (uint8_t i = 0; i < 5u; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool id_bus_is_stuck(const uint8_t id[5])
{
    bool all_same = true;
    for (uint8_t i = 1; i < 5u; ++i) {
        if (id[i] != id[0]) all_same = false;
    }
    return all_same ||
           (id[0] == 0x00u && id[1] == 0x00u && id[2] == 0x00u && id[3] == 0x00u && id[4] == 0x00u) ||
           (id[0] == 0xFFu && id[1] == 0xFFu && id[2] == 0xFFu && id[3] == 0xFFu && id[4] == 0xFFu);
}

static nand_result_t nand_check(void)
{
    nand_result_t result = {0};
    uint8_t baseline[5] = {0};
    uint8_t sample[5] = {0};
    bool baseline_valid = false;
    bool busy_seen = false;

    if ((FMC_Bank3->PCR & 0x04u) == 0u) result.fail_mask |= FAIL_FMC;

    result.rb_idle = rb_ready() ? 1u : 0u;
    if (!result.rb_idle) result.fail_mask |= FAIL_RB_IDLE;

    result.reset_ready = nand_reset(&busy_seen) ? 1u : 0u;
    result.rb_busy_seen = busy_seen ? 1u : 0u;
    if (!result.reset_ready) result.fail_mask |= FAIL_RESET;

    result.status = nand_read_status();
    if ((result.status & NAND_STATUS_READY) == 0u ||
        (result.status & NAND_STATUS_FAIL) != 0u) {
        result.fail_mask |= FAIL_STATUS;
    }
    if ((result.status & NAND_STATUS_WP_HIGH) == 0u) result.fail_mask |= FAIL_WP;

    for (uint8_t profile = 0; profile < TIMING_PROFILE_COUNT; ++profile) {
        nand_set_timing(timing_profiles[profile]);
        for (uint8_t check = 0; check < CHECKS_PER_TIMING; ++check) {
            nand_read_id(sample);
            if (!baseline_valid) {
                memcpy(baseline, sample, sizeof(baseline));
                baseline_valid = true;
            }
            if (bytes_equal(sample, supported_id_15) ||
                bytes_equal(sample, supported_id_95)) {
                result.expected_reads++;
            }
            if (bytes_equal(sample, baseline)) result.consistent_reads++;
            memcpy(result.id, sample, sizeof(result.id));
        }
    }
    nand_set_timing(timing_profiles[1]);

    if (result.expected_reads != TOTAL_ID_CHECKS) result.fail_mask |= FAIL_ID;
    if (result.consistent_reads != TOTAL_ID_CHECKS) result.fail_mask |= FAIL_STABILITY;
    if (id_bus_is_stuck(result.id)) result.fail_mask |= FAIL_BUS_STUCK;

    return result;
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
    case 'B': return "110101110101110";
    case 'C': return "111100100100111";
    case 'D': return "110101101101110";
    case 'E': return "111100110100111";
    case 'F': return "111100110100100";
    case 'G': return "111100101101111";
    case 'H': return "101101111101101";
    case 'I': return "111010010010111";
    case 'J': return "001001001101111";
    case 'K': return "101101110101101";
    case 'L': return "100100100100111";
    case 'M': return "101111111101101";
    case 'N': return "101111111111101";
    case 'O': return "111101101101111";
    case 'P': return "110101110100100";
    case 'Q': return "111101101111001";
    case 'R': return "110101110101101";
    case 'S': return "111100111001111";
    case 'T': return "111010010010010";
    case 'U': return "101101101101111";
    case 'V': return "101101101101010";
    case 'W': return "101101111111101";
    case 'X': return "101101010101101";
    case 'Y': return "101101010010010";
    case 'Z': return "111001010100111";
    case ':': return "000010000010000";
    case '/': return "001001010100100";
    case '-': return "000000111000000";
    case '.': return "000000000000010";
    default: return "000000000000000";
    }
}

static void ui_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey,
                    uint16_t color)
{
    volatile uint16_t *frame = (volatile uint16_t *)0xC0000000UL;
    const uint32_t stride = lcddev.width;

    if (lcddev.width == 0u || lcddev.height == 0u ||
        sx >= lcddev.width || sy >= lcddev.height) return;
    if (ex >= lcddev.width) ex = (uint16_t)(lcddev.width - 1u);
    if (ey >= lcddev.height) ey = (uint16_t)(lcddev.height - 1u);
    if (ex < sx || ey < sy) return;

    for (uint16_t y = sy; y <= ey; ++y) {
        volatile uint16_t *pixel = frame + (uint32_t)y * stride + sx;
        for (uint16_t x = sx; x <= ex; ++x) *pixel++ = color;
    }
    __DSB();
}

static void draw_cn_glyph(uint16_t x, uint16_t y, uint8_t glyph_id,
                          uint16_t color, uint8_t scale)
{
    const uint8_t *bitmap = nand_i18n_glyph_bitmap(glyph_id);
    if (bitmap == NULL || scale == 0u) return;

    for (uint8_t row = 0; row < NAND_I18N_GLYPH_HEIGHT; ++row) {
        for (uint8_t col = 0; col < NAND_I18N_GLYPH_WIDTH; ++col) {
            uint8_t packed = bitmap[row * NAND_I18N_GLYPH_ROW_BYTES + col / 8u];
            if ((packed & (uint8_t)(0x80u >> (col & 7u))) != 0u) {
                ui_fill((uint16_t)(x + col * scale),
                        (uint16_t)(y + row * scale),
                        (uint16_t)(x + col * scale + scale - 1u),
                        (uint16_t)(y + row * scale + scale - 1u),
                        color);
            }
        }
    }
}

static void draw_cn_text(uint16_t x, uint16_t y, const uint8_t *glyphs,
                         uint8_t glyph_count, uint16_t color, uint8_t scale)
{
    for (uint8_t i = 0; i < glyph_count; ++i) {
        draw_cn_glyph(x, y, glyphs[i], color, scale);
        x = (uint16_t)(x + (NAND_I18N_GLYPH_WIDTH + 2u) * scale);
    }
}

static void draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale)
{
    const char *pattern = tiny_pattern(c);
    for (uint8_t row = 0; row < 5u; ++row) {
        for (uint8_t col = 0; col < 3u; ++col) {
            if (pattern[row * 3u + col] == '1') {
                ui_fill((uint16_t)(x + col * scale),
                        (uint16_t)(y + row * scale),
                        (uint16_t)(x + col * scale + scale - 1u),
                        (uint16_t)(y + row * scale + scale - 1u),
                        color);
            }
        }
    }
}

static void draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t scale)
{
    while (*text != '\0') {
        draw_char(x, y, *text, color, scale);
        x = (uint16_t)(x + 4u * scale);
        ++text;
    }
}

static void draw_i18n(uint16_t x, uint16_t y, nand_text_id_t text_id,
                      uint16_t color, uint8_t chinese_scale,
                      uint8_t english_scale)
{
    const nand_i18n_text_t *text = nand_i18n_get(ui_language, text_id);
    if (ui_language == NAND_LANG_ZH_CN && text->glyphs != NULL) {
        draw_cn_text(x, y, text->glyphs, text->glyph_count, color, chinese_scale);
    } else {
        draw_text(x, y, text->ascii, color, english_scale);
    }
}

static void draw_static_screen(void)
{
    ui_fill(0, 0, (uint16_t)(lcddev.width - 1u),
            (uint16_t)(lcddev.height - 1u), UI_BG);
    draw_text(28, 18, "NAND", UI_TEXT, 4);
    draw_i18n(110, 14, NAND_TEXT_TITLE, UI_TEXT, 1, 4);
    draw_text((uint16_t)(lcddev.width - 118u), 18,
              nand_i18n_language_code(ui_language), UI_CYAN, 3);
    draw_i18n(30, 52, NAND_TEXT_READ_ONLY, UI_MUTED, 1, 2);
    ui_fill(24, 80, (uint16_t)(lcddev.width - 24u), 192, UI_YELLOW);
    draw_i18n(48, 104, NAND_TEXT_TESTING, BLACK, 2, 8);
}

static void draw_badge(uint16_t x, uint16_t y, uint16_t width,
                       const char *label, bool ok)
{
    ui_fill(x, y, (uint16_t)(x + width), (uint16_t)(y + 44u), ok ? UI_GREEN : UI_RED);
    draw_text((uint16_t)(x + 8u), (uint16_t)(y + 12u), label, UI_TEXT, 3);
}

static void draw_result(const nand_result_t *result)
{
    char line[80];
    const bool pass = result->fail_mask == 0u;
    const uint16_t width = lcddev.width;
    const uint16_t badge_width = (uint16_t)((width - 80u) / 8u);
    const uint16_t badge_gap = 6u;
    uint16_t x = 24u;

    ui_fill(24, 80, (uint16_t)(width - 24u), 192, pass ? UI_GREEN : UI_RED);
    draw_i18n(48, 104, pass ? NAND_TEXT_PASS : NAND_TEXT_FAIL, UI_TEXT, 2, 10);
    draw_i18n(260, 116,
              pass ? NAND_TEXT_CONNECTED : NAND_TEXT_CHECK_SOLDERING,
              UI_TEXT, 1, 4);

    ui_fill(24, 208, (uint16_t)(width - 24u), 404, UI_PANEL);
    draw_i18n(40, 222, NAND_TEXT_CHIP_ID, UI_CYAN, 1, 3);
    snprintf(line, sizeof(line), "%02X %02X %02X %02X %02X",
             result->id[0], result->id[1], result->id[2], result->id[3], result->id[4]);
    draw_text(170, 228, line, UI_CYAN, 4);

    draw_i18n(40, 262, NAND_TEXT_EXPECT, UI_MUTED, 1, 3);
    draw_text(170, 268, "EC DA 10 15 44", UI_MUTED, 3);

    draw_i18n(40, 296, NAND_TEXT_STATUS, UI_TEXT, 1, 3);
    snprintf(line, sizeof(line), "%02X", result->status);
    draw_text(128, 302, line, UI_TEXT, 3);
    draw_i18n(210, 296, NAND_TEXT_READY, UI_TEXT, 1, 3);
    draw_i18n(285, 296,
              (result->status & NAND_STATUS_READY) ? NAND_TEXT_YES : NAND_TEXT_NO,
              UI_TEXT, 1, 3);
    draw_i18n(390, 296, NAND_TEXT_WRITE_PROTECT, UI_TEXT, 1, 3);
    draw_i18n(570, 296,
              (result->status & NAND_STATUS_WP_HIGH) ? NAND_TEXT_HIGH : NAND_TEXT_LOW,
              UI_TEXT, 1, 3);

    draw_i18n(40, 334, NAND_TEXT_STABLE, UI_TEXT, 1, 3);
    snprintf(line, sizeof(line), "%03u/%03u", result->consistent_reads, TOTAL_ID_CHECKS);
    draw_text(125, 340, line, UI_TEXT, 3);
    draw_i18n(300, 334, NAND_TEXT_EXACT, UI_TEXT, 1, 3);
    snprintf(line, sizeof(line), "%03u/%03u", result->expected_reads, TOTAL_ID_CHECKS);
    draw_text(385, 340, line, UI_TEXT, 3);

    draw_i18n(40, 370, NAND_TEXT_CYCLES, UI_MUTED, 1, 3);
    snprintf(line, sizeof(line), "%06lu", (unsigned long)nand_diag_cycles);
    draw_text(125, 376, line, UI_MUTED, 3);
    draw_i18n(300, 370, NAND_TEXT_FAIL_MASK, UI_MUTED, 1, 3);
    snprintf(line, sizeof(line), "%08lX", (unsigned long)result->fail_mask);
    draw_text(420, 376, line, UI_MUTED, 3);

    draw_badge(x, 424, badge_width, "FMC", (result->fail_mask & FAIL_FMC) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "RB", (result->fail_mask & FAIL_RB_IDLE) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "RST", (result->fail_mask & FAIL_RESET) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "STA", (result->fail_mask & FAIL_STATUS) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "WP", (result->fail_mask & FAIL_WP) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "ID", (result->fail_mask & FAIL_ID) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "STB", (result->fail_mask & FAIL_STABILITY) == 0u);
    x = (uint16_t)(x + badge_width + badge_gap);
    draw_badge(x, 424, badge_width, "BUS", (result->fail_mask & FAIL_BUS_STUCK) == 0u);

    ui_fill(24, 486, (uint16_t)(width - 24u), 548, UI_PANEL);
    draw_text(40, 506, "R/B", UI_TEXT, 3);
    draw_i18n(90, 498, NAND_TEXT_IDLE, UI_TEXT, 1, 3);
    draw_i18n(150, 498, result->rb_idle ? NAND_TEXT_HIGH : NAND_TEXT_LOW,
              UI_TEXT, 1, 3);
    draw_i18n(250, 498, NAND_TEXT_RESET, UI_TEXT, 1, 3);
    draw_i18n(330, 498,
              result->reset_ready ? NAND_TEXT_NORMAL : NAND_TEXT_TIMEOUT,
              UI_TEXT, 1, 3);
    draw_i18n(470, 498, NAND_TEXT_BUSY_SIGNAL, UI_TEXT, 1, 3);
    draw_i18n(590, 498, result->rb_busy_seen ? NAND_TEXT_YES : NAND_TEXT_NO,
              UI_TEXT, 1, 3);
}

static bool poll_language_command(void)
{
    static char command[20];
    static uint8_t length = 0u;
    uint8_t byte;
    bool changed = false;

    for (uint8_t guard = 0; guard < 32u; ++guard) {
        if (HAL_UART_Receive(&UART1_Handler, &byte, 1u, 0u) != HAL_OK) break;
        if (byte == '\r' || byte == '\n') {
            if (length == 0u) continue;
            command[length] = '\0';
            if (strcmp(command, "LANG EN") == 0 || strcmp(command, "EN") == 0) {
                changed = ui_language != NAND_LANG_EN;
                ui_language = NAND_LANG_EN;
            } else if (strcmp(command, "LANG ZH") == 0 ||
                       strcmp(command, "LANG ZH-CN") == 0 ||
                       strcmp(command, "ZH") == 0) {
                changed = ui_language != NAND_LANG_ZH_CN;
                ui_language = NAND_LANG_ZH_CN;
            }
            nand_diag_language = (uint32_t)ui_language;
            printf("# language=%s\r\n", nand_i18n_language_code(ui_language));
            length = 0u;
        } else if (length + 1u < sizeof(command)) {
            if (byte >= 'a' && byte <= 'z') byte = (uint8_t)(byte - 'a' + 'A');
            command[length++] = (char)byte;
        } else {
            length = 0u;
        }
    }
    return changed;
}

static bool sync_external_language_request(void)
{
    uint32_t requested = nand_diag_language;

    if (requested > (uint32_t)NAND_LANG_EN) {
        nand_diag_language = (uint32_t)ui_language;
        return false;
    }
    if (requested == (uint32_t)ui_language) return false;

    ui_language = (nand_language_t)requested;
    printf("# language=%s source=external\r\n",
           nand_i18n_language_code(ui_language));
    return true;
}

static void publish_result(const nand_result_t *result)
{
    nand_diag_result = (result->fail_mask == 0u) ? 1u : 0u;
    nand_diag_fail_mask = result->fail_mask;
    nand_diag_id_0_3 = ((uint32_t)result->id[0]) |
                       ((uint32_t)result->id[1] << 8) |
                       ((uint32_t)result->id[2] << 16) |
                       ((uint32_t)result->id[3] << 24);
    nand_diag_id_4_status = ((uint32_t)result->id[4]) |
                            ((uint32_t)result->status << 8);
    nand_diag_rb_flags = ((uint32_t)result->rb_idle) |
                         ((uint32_t)result->rb_busy_seen << 1) |
                         ((uint32_t)result->reset_ready << 2);
    nand_diag_expected_reads = result->expected_reads;
    nand_diag_consistent_reads = result->consistent_reads;
    nand_diag_cycles++;
    if (result->fail_mask == 0u) nand_diag_pass_cycles++;
    else nand_diag_fail_cycles++;
    __DSB();
}

int main(void)
{
    nand_result_t result;
    bool fmc_ok;

    Cache_Enable();
    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4); /* 400 MHz core, same as sensor app. */
    delay_init(400);
    uart_init(115200);
    lamps_force_off();
    SDRAM_Init();
    LCD_Init();
    LCD_Display_Dir(1);
    draw_static_screen();

    fmc_ok = nand_bus_init();
    printf("# NAND read-only solder diagnostic\r\n");
    printf("# supported_ids='EC DA 10 15 44'|'EC DA 10 95 44' "
           "checks=%u no_erase=1 no_write=1\r\n",
           TOTAL_ID_CHECKS);
    printf("# language=%s commands='LANG ZH'|'LANG EN'\r\n",
           nand_i18n_language_code(ui_language));

    while (1) {
        bool language_changed = poll_language_command();
        if (sync_external_language_request()) language_changed = true;
        if (language_changed) draw_static_screen();
        result = nand_check();
        if (!fmc_ok) result.fail_mask |= FAIL_FMC;
        publish_result(&result);
        draw_result(&result);

        printf("NAND_DIAG,cycle=%lu,result=%s,id=%02X%02X%02X%02X%02X,status=%02X,"
               "rb_idle=%u,busy_seen=%u,reset=%u,exact=%u,stable=%u,fail_mask=%08lX\r\n",
               (unsigned long)nand_diag_cycles,
               result.fail_mask == 0u ? "PASS" : "FAIL",
               result.id[0], result.id[1], result.id[2], result.id[3], result.id[4],
               result.status, result.rb_idle, result.rb_busy_seen, result.reset_ready,
               result.expected_reads, result.consistent_reads,
               (unsigned long)result.fail_mask);

        HAL_Delay(750);
    }
}
