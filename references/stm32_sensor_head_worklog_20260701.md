# STM32 Sensor Head Worklog and Handoff

Date: 2026-07-01

This document records the code, tools, commands, wiring, problems, fixes, and current operating state for the STM32H743 sensor-head work.

## Scope

Goal:

```text
Use the STM32H743IIT6 board as an independent optical sensor head.
Read AS7343 spectrum and TSL2591 intensity.
Show live plots on the attached LCD.
Keep Arduino/ESP32/lamp control separate when needed.
```

The BaiduNetdisk STM32 package is reference-only:

```text
D:\BaiduNetdiskDownload\<STM32H743IIT6 vendor package>
```

Working repo:

```text
C:\Users\Administrator\Projects\stm32dev
```

## Main Firmware

Path:

```text
firmware/stm32_sensor_head_lcd
```

Main file:

```text
firmware/stm32_sensor_head_lcd/src/main.c
```

It includes:

- LCD/SDRAM startup from the vendor STM32 reference package.
- Startup image display from `startup_image.h`.
- Bit-banged I2C on GPIOB.
- AS7343 spectrum acquisition.
- TSL2591 intensity acquisition.
- Split-screen LCD visualization.

## Build and Burn Commands

Build:

```powershell
make -C firmware\stm32_sensor_head_lcd
```

Burn/flash:

```powershell
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program firmware/stm32_sensor_head_lcd/build/stm32_sensor_head_lcd.elf verify reset exit"
```

Rule:

```text
Any STM32 firmware source change must be followed by build + burn + commit + push.
```

## Hardware Wiring

STM32 I2C:

```text
STM32 3.3V -> sensor VCC/VIN
STM32 GND  -> sensor GND
STM32 PB8  -> sensor SCL
STM32 PB9  -> sensor SDA
```

TSL2591:

```text
address: 0x29
INT: leave unconnected
```

AS7343:

```text
address: 0x39
INT/GPIO: leave unconnected
```

ESP32-S3 fallback TSL wiring:

```text
ESP32-S3 IO9 -> TSL2591 SCL
ESP32-S3 IO8 -> TSL2591 SDA
ESP32-S3 3V3 -> TSL2591 VCC/VIN
ESP32-S3 GND -> TSL2591 GND
```

## Tools Used

Firmware build:

```text
arm-none-eabi-gcc
arm-none-eabi-objcopy
arm-none-eabi-size
make
```

STM32 flashing/debug:

```text
OpenOCD
ST-Link
arm-none-eabi-nm
OpenOCD memory reads: mdh, mdb, mdw
```

ESP32:

```text
arduino-cli
esptool
Python pyserial
```

Git:

```text
git add
git commit
git push
```

## Diagnostic Commands

Find live symbol addresses after every rebuild:

```powershell
arm-none-eabi-nm -n firmware\stm32_sensor_head_lcd\build\stm32_sensor_head_lcd.elf
```

Read STM32 live variables:

```powershell
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000" -c "init" -c "halt" -c "mdh <address> <count>" -c "resume" -c "shutdown"
```

Important globals:

```text
ok_as7343
ok_tsl
last_as
last_status2
last_tsl0
last_tsl1
seq
as_sample_count
tsl_sample_count
```

## Visualization Design

LCD orientation:

```c
LCD_Init();
LCD_Display_Dir(1);
```

Reason:

```text
Vendor LCD default was portrait, causing the plot to appear on the left half and rotated.
```

Text:

```text
Vendor text rendering was avoided because LCD_ShowChar/LCD_ShowString hard-faulted in the GCC build.
Use rectangles, lines, and color status boxes instead.
```

Layout:

```text
left panel  = AS7343 spectrum polyline
right panel = TSL2591 intensity traces
```

Right panel traces:

```text
orange = TSL full channel
green  = visible estimate, full - IR
```

Intensity display:

```text
The TSL plot uses a moving-average baseline.
Stable light sits near center.
Brightening moves up.
Dimming moves down.
```

## Acquisition Design

TSL2591:

```text
integration time = 100 ms
maximum meaningful new samples ~= 10 Hz
```

Reading faster only repeats the same sensor ADC result.

AS7343:

```text
ATIME = 5
ASTEP = 599
integration time ~= (5 + 1) * (599 + 1) * 2.78 us ~= 10 ms
read period target = 12 ms
```

Loop scheduling:

```text
AS7343 read period = 12 ms
TSL2591 read period = 100 ms
LCD draw period = 20 ms
```

This is not true simultaneous hardware parallelism because both sensors share one I2C bus. It is interleaved scheduling with separate sensor timing.

## Problems and Fixes

### Problem: Sensors not detected on STM32

Symptoms:

```text
ok_as7343 = 0
ok_tsl = 0
last_as[] = all zero
last_tsl0/last_tsl1 = zero
PB8/PB9 high
```

Fix/process:

```text
Verified firmware loop with seq.
Checked GPIOB IDR to confirm PB8/PB9 were not stuck low.
Tested swapped SDA/SCL.
Tested one sensor at a time.
Found breadboard/contact/power instability.
Direct wiring restored detection.
```

### Problem: TSL detected but values zero

Symptoms:

```text
ok_tsl = 1
last_tsl0 = 0
last_tsl1 = 0
target voltage once dropped to about 2.93 V
```

Fix/process:

```text
Rechecked sensor power and direct wiring.
Target voltage returned to about 3.21 V.
TSL values became nonzero.
```

### Problem: Plot looked like square wave without sensor

Cause:

```text
The firmware intentionally drew a red diagnostic heartbeat when both sensors were missing.
```

Fix:

```text
Once TSL was connected correctly, ok_tsl became 1 and real green/orange curves appeared.
```

### Problem: Plot only on left half and counterclockwise

Cause:

```text
LCD vendor driver defaulted to portrait.
```

Fix:

```c
LCD_Display_Dir(1);
```

### Problem: Bar plot flashed heavily

Cause:

```text
The firmware cleared and redrew large plot regions every frame.
```

Fix:

```text
Changed to line plots.
Only narrow columns/old line segments are erased.
```

### Problem: TSL lines pinned at top

Cause:

```text
Absolute autoscale mapped current maximum to the top.
```

Fix:

```text
Changed TSL display to moving-average-centered traces.
```

### Problem: AS7343 spectrum read was too slow

Cause:

```text
Original code restarted AS7343 measurement and waited 180 ms on every read.
```

Fix:

```text
Configured AS7343 once in continuous fast mode.
Reduced integration to about 10 ms.
Read data registers every 12 ms.
```

## ESP32-S3 TSL2591 Reader

Path:

```text
firmware/esp32_tsl2591_reader/esp32_tsl2591_reader.ino
```

Compile/upload:

```powershell
$fqbn='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,PSRAM=disabled'
arduino-cli compile --fqbn $fqbn firmware\esp32_tsl2591_reader
arduino-cli upload -p COM6 --fqbn $fqbn firmware\esp32_tsl2591_reader
```

Why it exists:

```text
It verified that the TSL2591 sensor and wiring were good independently of the STM32 path.
```

Confirmed output:

```text
# I2C: 0x29
# TSL2591 addr=0x29 present=1 configured=1
t_ms,present,configured,full,ir,visible
1077,1,1,299,89,210
```

## Current Validated State

After the latest STM32 burn:

```text
ok_as7343 = 1
ok_tsl    = 1
AS7343 channels = nonzero
TSL full  = 0x3587
TSL IR    = 0x0b48
target voltage ~= 3.21 V
```

Expected screen:

```text
left panel: live AS7343 spectrum curve
right panel: live TSL full/visible intensity curves centered around average
```

## Local Skill

Installed local skill:

```text
C:\Users\Administrator\.codex\skills\stm32-sensor-head
```

Purpose:

```text
Remember STM32 workflow rules, reference paths, burn requirement, and sensor-head debugging context.
```

Critical rule:

```text
Do not modify the BaiduNetdisk package.
Use it only as board/vendor reference.
All working code lives in the stm32dev repository.
```

