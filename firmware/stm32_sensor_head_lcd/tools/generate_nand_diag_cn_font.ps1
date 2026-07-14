param(
    [string]$FontName = "Microsoft YaHei",
    [int]$GlyphSize = 24,
    [int]$FontPixelSize = 22
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$glyphs = @(
    @{ Name = "CN_SHAN"; CodePoint = 0x95EA },
    @{ Name = "CN_CUN"; CodePoint = 0x5B58 },
    @{ Name = "CN_HAN"; CodePoint = 0x710A },
    @{ Name = "CN_JIE"; CodePoint = 0x63A5 },
    @{ Name = "CN_JIAN"; CodePoint = 0x68C0 },
    @{ Name = "CN_CE"; CodePoint = 0x6D4B },
    @{ Name = "CN_ZHI"; CodePoint = 0x53EA },
    @{ Name = "CN_DU"; CodePoint = 0x8BFB },
    @{ Name = "CN_BU"; CodePoint = 0x4E0D },
    @{ Name = "CN_CA"; CodePoint = 0x64E6 },
    @{ Name = "CN_CHU"; CodePoint = 0x9664 },
    @{ Name = "CN_XIE"; CodePoint = 0x5199 },
    @{ Name = "CN_RU"; CodePoint = 0x5165 },
    @{ Name = "CN_ZHENG"; CodePoint = 0x6B63 },
    @{ Name = "CN_ZAI"; CodePoint = 0x5728 },
    @{ Name = "CN_TONG"; CodePoint = 0x901A },
    @{ Name = "CN_GUO"; CodePoint = 0x8FC7 },
    @{ Name = "CN_SHI_LOSE"; CodePoint = 0x5931 },
    @{ Name = "CN_BAI"; CodePoint = 0x8D25 },
    @{ Name = "CN_LIAN"; CodePoint = 0x8FDE },
    @{ Name = "CN_CHANG"; CodePoint = 0x5E38 },
    @{ Name = "CN_QING"; CodePoint = 0x8BF7 },
    @{ Name = "CN_CHA"; CodePoint = 0x67E5 },
    @{ Name = "CN_XIN_CORE"; CodePoint = 0x82AF },
    @{ Name = "CN_PIAN"; CodePoint = 0x7247 },
    @{ Name = "CN_BIAN"; CodePoint = 0x7F16 },
    @{ Name = "CN_HAO"; CodePoint = 0x53F7 },
    @{ Name = "CN_YU"; CodePoint = 0x9884 },
    @{ Name = "CN_QI"; CodePoint = 0x671F },
    @{ Name = "CN_ZHUANG"; CodePoint = 0x72B6 },
    @{ Name = "CN_TAI"; CodePoint = 0x6001 },
    @{ Name = "CN_JIU"; CodePoint = 0x5C31 },
    @{ Name = "CN_XU"; CodePoint = 0x7EEA },
    @{ Name = "CN_BAO"; CodePoint = 0x4FDD },
    @{ Name = "CN_HU"; CodePoint = 0x62A4 },
    @{ Name = "CN_GAO"; CodePoint = 0x9AD8 },
    @{ Name = "CN_DI"; CodePoint = 0x4F4E },
    @{ Name = "CN_WEN"; CodePoint = 0x7A33 },
    @{ Name = "CN_DING"; CodePoint = 0x5B9A },
    @{ Name = "CN_JING"; CodePoint = 0x7CBE },
    @{ Name = "CN_QUE"; CodePoint = 0x786E },
    @{ Name = "CN_XUN"; CodePoint = 0x5FAA },
    @{ Name = "CN_HUAN"; CodePoint = 0x73AF },
    @{ Name = "CN_GU"; CodePoint = 0x6545 },
    @{ Name = "CN_ZHANG"; CodePoint = 0x969C },
    @{ Name = "CN_MA"; CodePoint = 0x7801 },
    @{ Name = "CN_KONG"; CodePoint = 0x7A7A },
    @{ Name = "CN_XIAN"; CodePoint = 0x95F2 },
    @{ Name = "CN_FU"; CodePoint = 0x590D },
    @{ Name = "CN_WEI_POSITION"; CodePoint = 0x4F4D },
    @{ Name = "CN_MANG"; CodePoint = 0x5FD9 },
    @{ Name = "CN_XIN_SIGNAL"; CodePoint = 0x4FE1 },
    @{ Name = "CN_SHI_YES"; CodePoint = 0x662F },
    @{ Name = "CN_FOU"; CodePoint = 0x5426 },
    @{ Name = "CN_CHAO"; CodePoint = 0x8D85 },
    @{ Name = "CN_SHI_TIME"; CodePoint = 0x65F6 }
)

$font = [System.Drawing.Font]::new(
    $FontName,
    [single]$FontPixelSize,
    [System.Drawing.FontStyle]::Regular,
    [System.Drawing.GraphicsUnit]::Pixel
)
$format = [System.Drawing.StringFormat]::new()
$format.Alignment = [System.Drawing.StringAlignment]::Center
$format.LineAlignment = [System.Drawing.StringAlignment]::Center
$format.FormatFlags = [System.Drawing.StringFormatFlags]::NoWrap

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine("#ifndef NAND_DIAG_CN_FONT_H")
[void]$builder.AppendLine("#define NAND_DIAG_CN_FONT_H")
[void]$builder.AppendLine("")
[void]$builder.AppendLine("#include <stdint.h>")
[void]$builder.AppendLine("")
[void]$builder.AppendLine("typedef enum {")
foreach ($glyph in $glyphs) {
    [void]$builder.AppendLine("    $($glyph.Name),")
}
[void]$builder.AppendLine("    CN_GLYPH_COUNT")
[void]$builder.AppendLine("} nand_cn_glyph_id_t;")
[void]$builder.AppendLine("")
[void]$builder.AppendLine("static const uint8_t nand_cn_glyphs[CN_GLYPH_COUNT][72] = {")

foreach ($glyph in $glyphs) {
    $bitmap = [System.Drawing.Bitmap]::new(
        $GlyphSize,
        $GlyphSize,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::Black)
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $graphics.DrawString(
        [string][char]$glyph.CodePoint,
        $font,
        [System.Drawing.Brushes]::White,
        [System.Drawing.RectangleF]::new(0.0, -1.0, $GlyphSize, $GlyphSize + 2.0),
        $format
    )

    [void]$builder.AppendLine("    [$($glyph.Name)] = {")
    for ($y = 0; $y -lt $GlyphSize; $y++) {
        $row = @()
        for ($byteIndex = 0; $byteIndex -lt 3; $byteIndex++) {
            $value = 0
            for ($bit = 0; $bit -lt 8; $bit++) {
                $x = $byteIndex * 8 + $bit
                if ($bitmap.GetPixel($x, $y).R -ge 48) {
                    $value = $value -bor (1 -shl (7 - $bit))
                }
            }
            $row += ('0x{0:X2}u' -f $value)
        }
        [void]$builder.AppendLine("        $($row -join ', '),")
    }
    [void]$builder.AppendLine("    },")
    $graphics.Dispose()
    $bitmap.Dispose()
}

[void]$builder.AppendLine("};")
[void]$builder.AppendLine("")
[void]$builder.AppendLine("#endif")

$font.Dispose()
$format.Dispose()

$firmwareRoot = Split-Path -Parent $PSScriptRoot
$output = Join-Path $firmwareRoot "src\nand_diag_cn_font.h"
[System.IO.File]::WriteAllText(
    $output,
    $builder.ToString(),
    [System.Text.UTF8Encoding]::new($false)
)
Write-Output $output
