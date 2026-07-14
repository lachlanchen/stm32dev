param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("zh", "zh-CN", "en")]
    [string]$Language
)

$ErrorActionPreference = "Stop"
$firmwareDir = Split-Path -Parent $PSScriptRoot
$elf = Join-Path $firmwareDir "build_nand_diag\stm32_nand_diag_lcd.elf"

if (-not (Test-Path -LiteralPath $elf)) {
    throw "Diagnostic ELF not found. Run: make -C firmware/stm32_sensor_head_lcd nand-diag"
}

$symbolLine = (& arm-none-eabi-nm -n $elf |
    Select-String -Pattern "\bnand_diag_language$" |
    Select-Object -Last 1).Line

if (-not $symbolLine) {
    throw "nand_diag_language was not found in the diagnostic ELF."
}

$address = ($symbolLine -split "\s+")[0]
$value = if ($Language -eq "en") { 1 } else { 0 }
$code = if ($value -eq 1) { "EN" } else { "ZH-CN" }

$previousPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& openocd `
    -f interface/stlink.cfg `
    -f target/stm32h7x.cfg `
    -c "adapter speed 1000" `
    -c "init" `
    -c "halt" `
    -c "mww 0x$address $value" `
    -c "resume" `
    -c "shutdown"
$openOcdExit = $LASTEXITCODE
$ErrorActionPreference = $previousPreference

if ($openOcdExit -ne 0) {
    throw "OpenOCD failed with exit code $openOcdExit. Check the ST-Link connection."
}

Write-Host "NAND diagnostic UI switched to $code. The display redraws automatically."
