# STM32F103 dual-SK6812 smooth hue demo

This independent bare-metal image preserves the verified
`stm32f103_sk6812_rgbw` firmware and adds a faster, smooth color-space demo for
two daisy-chained SK6812RGBW pixels on `PA0/A0`.

Both pixels display the same saturated hue. The integer HSV conversion follows:

```text
red -> yellow -> green -> cyan -> blue -> magenta -> red
```

The full wheel contains 1,536 hue positions: six transitions with 256 positions
each. A rational phase accumulator renders exactly 500 frames per cycle. It
alternates three- and four-position hue advances, updates at 1,000 frames/s,
and completes one wheel in exactly 0.5 seconds. A DWT deadline scheduler fixes
frame starts at 1 ms intervals instead of adding a delay after transmission.
The white channel stays at zero so it does not desaturate the RGB colors. Peak
demo brightness is limited to `96/255`.

## What 0-255 means

Each `R`, `G`, `B`, and `W` channel receives an unsigned 8-bit code:

- `0`: channel off;
- `1-254`: increasing internal PWM duty;
- `255`: maximum channel command.

These are digital brightness codes, not voltages. RGB alone has
`256^3 = 16,777,216` possible code combinations. RGBW accepts four bytes per
pixel, although many RGBW combinations produce similar perceived colors.

## Wiring

```text
STM32 PA0/A0 -> 330 ohm -> pixel 1 DI
pixel 1 DO ----------------> pixel 2 DI
regulated 5 V ------------> both pixel VCC pins
5 V supply GND -----------> both pixel GND pins and STM32 GND
```

A `74AHCT125` level buffer is recommended between the 3.3 V MCU output and a
pixel powered at 5 V.

## Build and flash only the F103 Telesky probe

```powershell
make -C firmware\stm32f103_sk6812_hue_cycle clean
make -C firmware\stm32f103_sk6812_hue_cycle
make -C firmware\stm32f103_sk6812_hue_cycle flash
```

The flash recipe selects USB PID `0483:3748` and SWD-DP ID `0x1ba01477`; it
cannot select the H7 onboard ST-Link/V2-1 (`0483:374B`).
