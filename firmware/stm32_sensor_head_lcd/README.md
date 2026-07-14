# STM32H743 sensor-head LCD firmware

This app separates measurement from lamp control. It reads AS7343 on I2C address `0x39` and TSL2591 on `0x29` using STM32 I2C1 (`PB8/PB9`), streams CSV on USART1/ST-Link VCP, and plots the latest spectrum/intensity on the attached LTDC screen.

Build:

```powershell
make -C firmware/stm32_sensor_head_lcd
```

Flash:

```powershell
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program firmware/stm32_sensor_head_lcd/build/stm32_sensor_head_lcd.elf verify reset exit"
```

Wiring:

| STM32 | Sensor bus |
|---|---|
| `3.3` | AS7343 VCC, TSL2591 VCC |
| `GND` | AS7343 GND, TSL2591 GND |
| `B8 / PB8` | AS7343 SCL, TSL2591 SCL |
| `B9 / PB9` | AS7343 SDA, TSL2591 SDA |
| open | AS7343 GPIO/INT, TSL2591 INT |

The current AS7343 code uses register-level 18-channel auto-SMUX mode copied from the proven Arduino `as7343_uno_chunked` workflow.

## Read-only NAND solder diagnostic

`src/nand_diag_main.c` is an independent diagnostic entry point for the
Samsung `K9F2G08U0C-SIB0`. The normal `src/main.c` sensor application is not
modified or linked into this image. The diagnostic forces lamp outputs
`PA0/PA1` low, initializes FMC Bank 3, and continuously displays:

- exact five-byte ID comparison against `EC DA 10 15 44`;
- 96 ID reads across three conservative FMC timing profiles;
- ID stability, stuck-bus detection, R/B status, reset completion, and WP state;
- a green `PASS` or red `FAIL` screen with a bit-coded failure mask.

It is non-destructive: only NAND commands `RESET (FF)`, `READ STATUS (70)`,
and `READ ID (90)` are present. It never programs or erases NAND data.

Build the separate image:

```powershell
make -C firmware/stm32_sensor_head_lcd nand-diag
```

Flash the diagnostic image:

```powershell
make -C firmware/stm32_sensor_head_lcd flash-nand-diag
```

Restore the normal sensor-head firmware at any time:

```powershell
make -C firmware/stm32_sensor_head_lcd flash
```

### Diagnostic UI languages

The NAND diagnostic uses a small key-based i18n layer. It starts in Simplified
Chinese (`zh-CN`) and keeps a complete English fallback. Send either command on
USART1 at 115200 baud to switch immediately without rebuilding:

```text
LANG ZH
LANG EN
```

Chinese labels use an embedded 24-by-24 bitmap font, so the firmware does not
depend on an SD card or the vendor text renderer. Regenerate the subset after
adding translations with:

```powershell
powershell -ExecutionPolicy Bypass -File firmware/stm32_sensor_head_lcd/tools/generate_nand_diag_cn_font.ps1
```
