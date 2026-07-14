# STM32F103 SK6812RGBW color test

This independent bare-metal image targets the newly detected Cortex-M3 board:

- STM32 device ID: `0x410`
- flash: `128 KiB`
- data output: `PA0 / A0`
- debugger: Telesky ST-Link/V2, USB `0483:3748`

It repeatedly displays off, red, green, blue, and the dedicated white channel.
Each illuminated color lasts one second at `48/255` bring-up intensity.

## Wiring

With one pixel:

```text
STM32 PA0/A0 -> 330 ohm -> SK6812 DI
STM32 GND ----------------> SK6812 GND
regulated 5 V -----------> SK6812 VCC
5 V supply GND ----------> STM32 GND
SK6812 DO ----------------> unconnected
```

For another pixel, connect the first pixel `DO` to the second pixel `DI`; keep
all VCC and GND connections in parallel. A 74AHCT125 level buffer is recommended
between 3.3 V PA0 and a pixel powered at 5 V.

## Build and flash only the external Telesky probe

```powershell
make -C firmware\stm32f103_sk6812_rgbw
make -C firmware\stm32f103_sk6812_rgbw flash
```

The flash command filters for USB PID `3748`, so it does not select the old
H743 board's onboard ST-Link/V2-1 (`374B`).
