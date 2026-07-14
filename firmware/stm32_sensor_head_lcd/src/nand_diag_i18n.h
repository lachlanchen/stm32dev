#ifndef NAND_DIAG_I18N_H
#define NAND_DIAG_I18N_H

#include <stddef.h>
#include <stdint.h>

#define NAND_I18N_GLYPH_WIDTH       24u
#define NAND_I18N_GLYPH_HEIGHT      24u
#define NAND_I18N_GLYPH_ROW_BYTES   3u

typedef enum {
    NAND_LANG_ZH_CN = 0,
    NAND_LANG_EN = 1,
    NAND_LANG_COUNT
} nand_language_t;

typedef enum {
    NAND_TEXT_TITLE = 0,
    NAND_TEXT_READ_ONLY,
    NAND_TEXT_TESTING,
    NAND_TEXT_PASS,
    NAND_TEXT_FAIL,
    NAND_TEXT_CONNECTED,
    NAND_TEXT_CHECK_SOLDERING,
    NAND_TEXT_CHIP_ID,
    NAND_TEXT_EXPECT,
    NAND_TEXT_STATUS,
    NAND_TEXT_READY,
    NAND_TEXT_WRITE_PROTECT,
    NAND_TEXT_STABLE,
    NAND_TEXT_EXACT,
    NAND_TEXT_CYCLES,
    NAND_TEXT_FAIL_MASK,
    NAND_TEXT_IDLE,
    NAND_TEXT_RESET,
    NAND_TEXT_BUSY_SIGNAL,
    NAND_TEXT_HIGH,
    NAND_TEXT_LOW,
    NAND_TEXT_YES,
    NAND_TEXT_NO,
    NAND_TEXT_NORMAL,
    NAND_TEXT_TIMEOUT,
    NAND_TEXT_COUNT
} nand_text_id_t;

typedef struct {
    const char *ascii;
    const uint8_t *glyphs;
    uint8_t glyph_count;
} nand_i18n_text_t;

const nand_i18n_text_t *nand_i18n_get(nand_language_t language,
                                      nand_text_id_t text_id);
const uint8_t *nand_i18n_glyph_bitmap(uint8_t glyph_id);
const char *nand_i18n_language_code(nand_language_t language);

#endif
