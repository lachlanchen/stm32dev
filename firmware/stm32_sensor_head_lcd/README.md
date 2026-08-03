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

Lamp and addressable-pixel outputs:

| STM32 | Device |
|---|---|
| `A2 / PA2` | 12 V tungsten MOS PWM input |
| `A3 / PA3` | Single WS2812B `DIN` through a 220-470 ohm series resistor |
| `GND` | Shared STM32, 5 V pixel supply, and MOS logic ground |

The pixel uses an external regulated 5 V supply. Never connect it to the 12 V
tungsten supply. A 5 V AHCT data buffer is recommended; direct 3.3 V data may
work for a short test but is not guaranteed. The current diagnostic firmware
transmits one SK6812 RGBW 32-bit `GRBW` red frame, using approximately 300 ns
`T0H`, 600 ns `T1H`, and a 300 us reset interval, then leaves the data line low.
SK6812 retains the latched color without periodic refresh. Reset the MCU after
reconnecting a loose GND or DI wire.
The PA3 waveform is emitted as a short GPIO/DWT transaction, so TIM2 continues
running the tungsten-lamp PWM while pixel data is sent.

The current AS7343 code uses register-level 18-channel auto-SMUX mode copied from the proven Arduino `as7343_uno_chunked` workflow.

## Read-only NAND solder diagnostic

Detailed Chinese hardware, GUI, memory-map, performance, and OS/ROS usage guide:
[NAND diagnostic and board memory architecture](../../references/nand-diagnostic-memory-architecture-cn.md).

`src/nand_diag_main.c` is an independent diagnostic entry point for the
Samsung `K9F2G08U0C-SIB0`. The normal `src/main.c` sensor application is not
modified or linked into this image. The diagnostic forces lamp outputs
`PA0/PA1` low, initializes FMC Bank 3, and continuously displays:

- exact five-byte ID comparison against the documented/observed signatures
  `EC DA 10 15 44` and `EC DA 10 95 44`;
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

## Read-only W25Q64 QSPI diagnostic

`src/qspi_diag_main.c` is an independent, non-destructive W25Q64 detector. It
reads JEDEC ID, status registers, SFDP, Unique ID, and four bytes at address 0;
it contains no write-enable, program, erase, or status-write command.

```powershell
make -C firmware/stm32_sensor_head_lcd qspi-diag
```

The installed device passed as Winbond W25Q64 (`EF 40 17`) with 64/64 stable
reads and valid SFDP. See
[the Chinese QSPI detection record](../../references/w25q64-qspi-detection-cn.md).

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

## Independent SK6812RGBW two-pixel runner

`src/sk6812_rgbw_main.c` is a separate image for a second STM32H743 board. It
drives two daisy-chained SK6812RGBW pixels from `PA0/A0`; it does not replace or
modify the TSL2591/AS7343 source in `src/main.c`.

```powershell
make -C firmware/stm32_sensor_head_lcd sk6812
make -C firmware/stm32_sensor_head_lcd flash-sk6812
```

Restore the sensor-head image with `make -C firmware/stm32_sensor_head_lcd flash`.
Use an external 5 V supply, common ground, a 5 V AHCT-level buffer, local
decoupling, and connect first-pixel `DOUT` to second-pixel `DIN`. See the
[Chinese wiring tutorial](../../publications/sk6812_rgbw_stm32_cn/sk6812_rgbw_stm32_cn.pdf).

## Independent PA3 one-pixel test

`src/sk6812_pa3_test_main.c` is a minimal image for one SK6812RGBW connected to
`PA3/A3`. It loops through red, green, blue, and white, holding each color for
three seconds. Both the color hold and the pixel waveform use fixed 400 MHz DWT
timing, independent of the HAL tick. Each color frame is transmitted once and
then remains latched. It uses a 32-bit `GRBW` frame. The normal LCD/sensor
source remains separate and is not linked into this test image; the LCD is
intentionally not initialized.

```powershell
make -C firmware\stm32_sensor_head_lcd sk6812-pa3-test
make -C firmware\stm32_sensor_head_lcd flash-sk6812-pa3-test
```

Restore the unchanged LCD/sensor application with:

```powershell
make -C firmware\stm32_sensor_head_lcd flash
```

## Independent PA3 TIM2 hardware-waveform test

`src/sk6812_pa3_timer_test_main.c` is the deterministic follow-up to the DWT
bit-bang test. TIM2_CH4 drives `PA3/A3` with a 200 MHz timer: `ARR=249`, zero-bit
high time `60` ticks, and one-bit high time `120` ticks. CCR4 preload removes
software edge jitter. It sends identical `GRBW` frames to two daisy-chained
pixels and holds red, green, blue, and white for three seconds each.
After every frame, PA3 is changed back to push-pull GPIO output-low for the
entire three-second hold. This prevents the data input from floating while
TIM2_CH4 is disabled; PA3 returns to alternate-function mode only while a frame
is being transmitted.

```powershell
make -C firmware\stm32_sensor_head_lcd sk6812-pa3-timer-test
make -C firmware\stm32_sensor_head_lcd flash-sk6812-pa3-timer-test
```

Only one MCU output may drive the pixel data input. Disconnect the F103 PA0
wire before attaching H7 PA3. If this hardware-timed image still flickers, the
remaining fault is electrical: shared ground, loose DI, 5 V supply integrity,
or the need for a `74AHCT125` 3.3-to-5 V data buffer.
