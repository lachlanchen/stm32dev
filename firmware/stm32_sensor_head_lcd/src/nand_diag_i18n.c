#include "nand_diag_i18n.h"
#include "nand_diag_cn_font.h"

#define GLYPH_COUNT(sequence) ((uint8_t)(sizeof(sequence) / sizeof((sequence)[0])))
#define ZH_ENTRY(fallback, sequence) {fallback, sequence, GLYPH_COUNT(sequence)}
#define EN_ENTRY(text) {text, NULL, 0u}

static const uint8_t zh_title[] = {
    CN_SHAN, CN_CUN, CN_HAN, CN_JIE, CN_JIAN, CN_CE
};
static const uint8_t zh_read_only[] = {
    CN_ZHI, CN_DU, CN_JIAN, CN_CE, CN_BU, CN_CA, CN_CHU,
    CN_BU, CN_XIE, CN_RU
};
static const uint8_t zh_testing[] = {CN_ZHENG, CN_ZAI, CN_JIAN, CN_CE};
static const uint8_t zh_pass[] = {CN_TONG, CN_GUO};
static const uint8_t zh_fail[] = {CN_SHI_LOSE, CN_BAI};
static const uint8_t zh_connected[] = {CN_LIAN, CN_JIE, CN_ZHENG, CN_CHANG};
static const uint8_t zh_check_soldering[] = {
    CN_QING, CN_JIAN, CN_CHA, CN_HAN, CN_JIE
};
static const uint8_t zh_chip_id[] = {CN_XIN_CORE, CN_PIAN, CN_BIAN, CN_HAO};
static const uint8_t zh_expect[] = {CN_YU, CN_QI};
static const uint8_t zh_status[] = {CN_ZHUANG, CN_TAI};
static const uint8_t zh_ready[] = {CN_JIU, CN_XU};
static const uint8_t zh_write_protect[] = {CN_XIE, CN_BAO, CN_HU};
static const uint8_t zh_stable[] = {CN_WEN, CN_DING};
static const uint8_t zh_exact[] = {CN_JING, CN_QUE};
static const uint8_t zh_cycles[] = {CN_XUN, CN_HUAN};
static const uint8_t zh_fail_mask[] = {CN_GU, CN_ZHANG, CN_MA};
static const uint8_t zh_idle[] = {CN_KONG, CN_XIAN};
static const uint8_t zh_reset[] = {CN_FU, CN_WEI_POSITION};
static const uint8_t zh_busy_signal[] = {CN_MANG, CN_XIN_SIGNAL, CN_HAO};
static const uint8_t zh_high[] = {CN_GAO};
static const uint8_t zh_low[] = {CN_DI};
static const uint8_t zh_yes[] = {CN_SHI_YES};
static const uint8_t zh_no[] = {CN_FOU};
static const uint8_t zh_normal[] = {CN_ZHENG, CN_CHANG};
static const uint8_t zh_timeout[] = {CN_CHAO, CN_SHI_TIME};

static const nand_i18n_text_t zh_cn[NAND_TEXT_COUNT] = {
    [NAND_TEXT_TITLE] = ZH_ENTRY("FLASH SOLDER CHECK", zh_title),
    [NAND_TEXT_READ_ONLY] = ZH_ENTRY("READ ONLY NO ERASE NO WRITE", zh_read_only),
    [NAND_TEXT_TESTING] = ZH_ENTRY("TESTING", zh_testing),
    [NAND_TEXT_PASS] = ZH_ENTRY("PASS", zh_pass),
    [NAND_TEXT_FAIL] = ZH_ENTRY("FAIL", zh_fail),
    [NAND_TEXT_CONNECTED] = ZH_ENTRY("NAND CONNECTED", zh_connected),
    [NAND_TEXT_CHECK_SOLDERING] = ZH_ENTRY("CHECK SOLDERING", zh_check_soldering),
    [NAND_TEXT_CHIP_ID] = ZH_ENTRY("CHIP ID", zh_chip_id),
    [NAND_TEXT_EXPECT] = ZH_ENTRY("EXPECT", zh_expect),
    [NAND_TEXT_STATUS] = ZH_ENTRY("STATUS", zh_status),
    [NAND_TEXT_READY] = ZH_ENTRY("READY", zh_ready),
    [NAND_TEXT_WRITE_PROTECT] = ZH_ENTRY("WRITE PROTECT", zh_write_protect),
    [NAND_TEXT_STABLE] = ZH_ENTRY("STABLE", zh_stable),
    [NAND_TEXT_EXACT] = ZH_ENTRY("EXACT", zh_exact),
    [NAND_TEXT_CYCLES] = ZH_ENTRY("CYCLES", zh_cycles),
    [NAND_TEXT_FAIL_MASK] = ZH_ENTRY("FAIL MASK", zh_fail_mask),
    [NAND_TEXT_IDLE] = ZH_ENTRY("IDLE", zh_idle),
    [NAND_TEXT_RESET] = ZH_ENTRY("RESET", zh_reset),
    [NAND_TEXT_BUSY_SIGNAL] = ZH_ENTRY("BUSY SIGNAL", zh_busy_signal),
    [NAND_TEXT_HIGH] = ZH_ENTRY("HIGH", zh_high),
    [NAND_TEXT_LOW] = ZH_ENTRY("LOW", zh_low),
    [NAND_TEXT_YES] = ZH_ENTRY("YES", zh_yes),
    [NAND_TEXT_NO] = ZH_ENTRY("NO", zh_no),
    [NAND_TEXT_NORMAL] = ZH_ENTRY("NORMAL", zh_normal),
    [NAND_TEXT_TIMEOUT] = ZH_ENTRY("TIMEOUT", zh_timeout),
};

static const nand_i18n_text_t en[NAND_TEXT_COUNT] = {
    [NAND_TEXT_TITLE] = EN_ENTRY("FLASH SOLDER CHECK"),
    [NAND_TEXT_READ_ONLY] = EN_ENTRY("READ ONLY NO ERASE NO WRITE"),
    [NAND_TEXT_TESTING] = EN_ENTRY("TESTING"),
    [NAND_TEXT_PASS] = EN_ENTRY("PASS"),
    [NAND_TEXT_FAIL] = EN_ENTRY("FAIL"),
    [NAND_TEXT_CONNECTED] = EN_ENTRY("NAND CONNECTED"),
    [NAND_TEXT_CHECK_SOLDERING] = EN_ENTRY("CHECK SOLDERING"),
    [NAND_TEXT_CHIP_ID] = EN_ENTRY("CHIP ID"),
    [NAND_TEXT_EXPECT] = EN_ENTRY("EXPECT"),
    [NAND_TEXT_STATUS] = EN_ENTRY("STATUS"),
    [NAND_TEXT_READY] = EN_ENTRY("READY"),
    [NAND_TEXT_WRITE_PROTECT] = EN_ENTRY("WRITE PROTECT"),
    [NAND_TEXT_STABLE] = EN_ENTRY("STABLE"),
    [NAND_TEXT_EXACT] = EN_ENTRY("EXACT"),
    [NAND_TEXT_CYCLES] = EN_ENTRY("CYCLES"),
    [NAND_TEXT_FAIL_MASK] = EN_ENTRY("FAIL MASK"),
    [NAND_TEXT_IDLE] = EN_ENTRY("IDLE"),
    [NAND_TEXT_RESET] = EN_ENTRY("RESET"),
    [NAND_TEXT_BUSY_SIGNAL] = EN_ENTRY("BUSY SIGNAL"),
    [NAND_TEXT_HIGH] = EN_ENTRY("HIGH"),
    [NAND_TEXT_LOW] = EN_ENTRY("LOW"),
    [NAND_TEXT_YES] = EN_ENTRY("YES"),
    [NAND_TEXT_NO] = EN_ENTRY("NO"),
    [NAND_TEXT_NORMAL] = EN_ENTRY("NORMAL"),
    [NAND_TEXT_TIMEOUT] = EN_ENTRY("TIMEOUT"),
};

const nand_i18n_text_t *nand_i18n_get(nand_language_t language,
                                      nand_text_id_t text_id)
{
    if ((unsigned)text_id >= NAND_TEXT_COUNT) text_id = NAND_TEXT_TITLE;
    if (language == NAND_LANG_ZH_CN && zh_cn[text_id].glyphs != NULL) {
        return &zh_cn[text_id];
    }
    return &en[text_id];
}

const uint8_t *nand_i18n_glyph_bitmap(uint8_t glyph_id)
{
    if (glyph_id >= CN_GLYPH_COUNT) return NULL;
    return nand_cn_glyphs[glyph_id];
}

const char *nand_i18n_language_code(nand_language_t language)
{
    return language == NAND_LANG_EN ? "EN" : "ZH-CN";
}
