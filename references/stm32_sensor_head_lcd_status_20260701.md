# STM32 Sensor Head LCD Status, 2026-07-01

## Goal

Move optical measurement away from the Arduino lamp controller and onto the STM32H743IIT6 display board. The STM32 should read:

- AS7343 spectrum sensor over I2C
- TSL2591 intensity sensor over I2C
- Live plot/status directly on the attached LCD

## Flashed Firmware

Firmware path:

```text
firmware/stm32_sensor_head_lcd
```

Build and flash:

```powershell
make -C firmware\stm32_sensor_head_lcd
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program firmware/stm32_sensor_head_lcd/build/stm32_sensor_head_lcd.elf verify reset exit"
```

The firmware initializes SDRAM/LCD, shows the startup image, then draws a bar-style sensor screen without using the vendor text renderer, because the GCC build hard-faulted inside the vendor `LCD_ShowChar` path.

## I2C Modes Tested Automatically

The firmware bit-bangs I2C on GPIOB and continuously re-probes once per second if either sensor is missing.

It tries:

| Mode | SCL | SDA |
| --- | --- | --- |
| 0 | PB8 | PB9 |
| 1 | PB9 | PB8 |
| 2 | PB6 | PB7 |
| 3 | PB7 | PB6 |

This means the firmware can recover after swapping SDA/SCL or reconnecting the sensor while the board is already running.

## Current SWD Result

After flashing, OpenOCD verified the image and the STM32 target voltage was about `3.25 V`.

Live memory check:

```text
bb_scl_pin = GPIO_PIN_8
bb_sda_pin = GPIO_PIN_9
i2c_mode   = 0
ok_as7343  = 0
ok_tsl     = 0
seq        incrementing
last_as[]  = all zero
last_tsl0  = 0
last_tsl1  = 0
```

Interpretation:

- The STM32 firmware is running.
- The screen drawing path is active.
- The I2C bus is not stuck low.
- Neither AS7343 at `0x39` nor TSL2591 at `0x29` is ACKing on the tested pin pairs.

## Exact Wiring Needed

Use the STM32 board power domain, not Arduino power, for this measurement head:

```text
STM32 3.3V  -> AS7343 VCC/VIN
STM32 GND   -> AS7343 GND
STM32 PB8   -> AS7343 SCL
STM32 PB9   -> AS7343 SDA

STM32 3.3V  -> TSL2591 VCC/VIN
STM32 GND   -> TSL2591 GND
STM32 PB8   -> TSL2591 SCL
STM32 PB9   -> TSL2591 SDA

AS7343 INT/GPIO -> leave unconnected for now
TSL2591 INT     -> leave unconnected for now
```

Both sensors share the same I2C bus. Connect all sensor grounds to STM32 GND.

## LCD Status Meaning

Because text rendering is disabled, the LCD uses colored boxes:

- First box: AS7343 status, green means detected/readable, red means missing
- Second box: TSL2591 status, green means detected/readable, red means missing
- Four small boxes: selected I2C mode, cyan indicates current mode
- Last box: SDA idle level, green means high/released, red means low/stuck

If wiring is corrected while firmware is running, the boxes should turn green automatically after the next one-second probe.

## 2026-07-01 Line Plot Update

The first LCD build redrew the whole plot region every frame, which caused visible flashing and made missing sensors look like a blank bar plot.

The current firmware uses a persistent oscilloscope-style line plot:

- Cyan trace: AS7343 selected-channel spectral sum
- Green trace: TSL2591 visible intensity estimate, `full - IR`
- Red bottom trace: diagnostic heartbeat only when both sensors are missing

The firmware now erases only a narrow vertical column and draws the next line segment. This avoids full-screen or full-panel flicker and makes the board visibly alive even before the sensors ACK.

## 2026-07-01 Split-Screen Update

The current flashed version separates the two measurements spatially:

- Left panel: AS7343 spectral shape as a connected polyline across the selected channels
- Right panel: TSL2591 visible-intensity trace over time

The display no longer mixes spectrum and intensity into one plot. If AS7343 is missing, the left spectral line is drawn in red. If TSL2591 is missing, the right intensity line is drawn in red. If both are missing, the right panel also shows a red heartbeat trace so the screen still proves the firmware loop is alive.

Live SWD detection after flashing the split-screen firmware:

```text
i2c_mode   = 0
SCL/SDA    = PB8/PB9
ok_as7343  = 0
ok_tsl     = 0
last_as[]  = all zero
last_tsl0  = 0
last_tsl1  = 0
seq        = incrementing
```

Conclusion: the STM32 firmware and LCD visualization work, but the sensors are still not ACKing on the tested I2C pin modes. Correct wiring should make the status boxes and traces turn live without reflashing.

## 2026-07-01 Landscape and Slow-I2C Update

The panel was observed drawing only on the left side and rotated counterclockwise. The firmware now forces landscape mode after LCD initialization:

```c
LCD_Init();
LCD_Display_Dir(1);
```

The bit-banged I2C delay was also slowed from `5 us` to `20 us` to help with weak pullups or long jumpers.

Live SWD detection after this update:

```text
SCL/SDA pins = PB8/PB9
i2c_mode    = 0
ok_as7343   = 0
ok_tsl      = 0
last_as[]   = all zero
last_tsl0   = 0
last_tsl1   = 0
seq         = incrementing
GPIOB IDR   = 0x000003f0
```

`GPIOB IDR = 0x000003f0` means PB8 and PB9 are electrically high/released, so the bus is not stuck low. The remaining failure is no ACK from either sensor. If the user confirms the jumpers are on PB8/PB9, the next likely causes are sensor VCC/GND, wrong physical header row, sensor board not powered, missing common ground, or address/module mismatch.

## 2026-07-01 Refined TSL2591 Acquisition

The STM32 firmware now keeps TSL2591 at the fastest meaningful integration time:

```text
integration time: 100 ms
maximum new optical samples: about 10 Hz
```

Reading faster than this repeats the same ADC result, so the firmware prioritizes clean live visualization instead of fake oversampling.

The TSL2591 path now uses simple auto-gain:

```text
peak > 60000 counts -> lower gain
peak < 512 counts   -> raise gain
```

The right-side plot now has two intensity traces:

```text
orange = full-spectrum TSL channel
green  = visible estimate = full - IR
```

The Baidu Netdisk STM32 package remains read-only reference material. Working firmware lives in:

```text
C:\Users\Administrator\Projects\stm32dev\firmware\stm32_sensor_head_lcd
```

## 2026-07-01 Centered Intensity Visualization

The TSL intensity traces previously used absolute autoscale. When the current reading became the local maximum, the line appeared pinned near the top of the plot.

The current firmware uses a centered, baseline-tracking display for the right-side TSL plot:

```text
center line = slow moving average
orange      = full channel deviation around average
green       = visible estimate deviation around average
```

This makes stable light sit near the middle of the panel. Brightening moves the trace upward; dimming moves it downward. The absolute raw values are still preserved in `last_tsl0` and `last_tsl1`.

## 2026-07-01 Interleaved Hardware-Limit Spectrum/Intensity Readout

The acquisition loop now separates sensor timing from LCD drawing:

```text
AS7343 spectrum read period: 12 ms target
TSL2591 intensity read period: 100 ms target
LCD draw period: 20 ms target
```

This is not true electrical parallelism because both sensors share one I2C bus, but it is parallel-style scheduling: the firmware no longer blocks the whole loop for each spectrum frame.

AS7343 was changed from blocking one-shot mode to continuous fast mode:

```text
old: restart AS7343 measurement + wait 180 ms every spectrum read
new: configure once, read data registers on a 12 ms schedule
```

AS7343 integration timing follows the datasheet equation:

```text
integration time = (ATIME + 1) * (ASTEP + 1) * 2.78 us
ATIME = 5
ASTEP = 599
integration time ~= 10 ms
```

The AS7343 path also uses simple auto-gain:

```text
peak > 85% of current fast full scale -> lower gain
peak < 15% of current fast full scale -> raise gain
```

TSL2591 remains at its fastest meaningful mode:

```text
integration time = 100 ms
new optical samples ~= 10 Hz
```

Reading TSL2591 faster than this only repeats the same ADC result.

Live validation after flashing:

```text
ok_as7343 = 1
ok_tsl    = 1
AS7343 channels = nonzero
TSL full  = 0x3587
TSL IR    = 0x0b48
AS samples after boot = 0x353
target voltage = about 3.21 V
```

The left spectrum plot should now update from real AS7343 data, and the right intensity plot should update from real TSL2591 data.

## 2026-07-01 Full AS7343 14-Point Spectrum and Robust TSL Display

The AS7343 product is a 14-channel spectral sensor: visible/NIR spectral channels plus clear and flicker-related data. In the firmware's 18-register auto-SMUX mode, the useful order is:

```text
0  FZ 450 nm
1  FY 555 nm
2  FXL 600 nm
3  NIR 855 nm
4  VIS/Clear cycle 1
5  FD cycle 1
6  F2 425 nm
7  F3 475 nm
8  F4 515 nm
9  F6 640 nm
10 VIS/Clear cycle 2
11 FD cycle 2
12 F1 405 nm
13 F7 690 nm
14 F8 745 nm
15 F5 550 nm
16 VIS/Clear cycle 3
17 FD cycle 3
```

The left panel now draws 14 points:

```text
F1, F2, FZ, F3, F4, F5, FY, FXL, F6, F7, F8, NIR, ClearAvg, FDAvg
```

`ClearAvg` is the average of VIS/Clear cycles 1, 2, and 3. `FDAvg` is the average of FD cycles 1, 2, and 3.

The top status boxes mean:

```text
box 1 green/red  = AS7343 detected/readable
box 2 green/red  = TSL2591 detected/readable
cyan + 3 grey    = selected I2C pin mode; cyan mode 0 means PB8=SCL, PB9=SDA
last green/red   = SDA idle level, green means high/released
```

The right intensity panel has two lines:

```text
orange = TSL2591 full channel, visible + infrared
green  = TSL2591 visible estimate, full - infrared
```

The TSL display now uses gain-normalized values, so auto-gain changes should not create artificial jumps in the plotted curves. Raw `full` and `IR` ADC values are still preserved and printed in CSV.

The CSV now includes:

```text
raw TSL full/IR
raw visible estimate
TSL gain code
gain-normalized full/visible
TSL sample counter
AS7343 gain code
AS7343 sample counter
all 18 raw AS7343 registers
14-point display sum
```

Live validation after flashing this version:

```text
ok_as7343 = 1
ok_tsl    = 1
AS7343 raw registers = nonzero
TSL raw/normalized values = nonzero
AS7343 sample counter = increasing
TSL sample counter = increasing
target voltage = about 3.21 V
```

Reference notes used for this update:

```text
ams OSRAM AS7343 datasheet:
- AS7343 is a 14-channel multispectral sensor.
- Integration time = (ATIME + 1) * (ASTEP + 1) * 2.78 us.
- Auto-SMUX 18-channel readout stores FZ/FY/FXL/NIR/VIS/FD and F1-F8 across three cycles.

ams OSRAM TSL2591 datasheet:
- Channel 0 is broadband/full, visible + IR.
- Channel 1 is IR.
- Two integrating ADCs produce the digital irradiance channels.
```
