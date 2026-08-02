# STM32F103 SK6812RGBW color test

This independent bare-metal image targets the newly detected Cortex-M3 board:

- STM32 device ID: `0x410`
- flash: `128 KiB`
- data output: `PA0 / A0`
- debugger: Telesky ST-Link/V2, USB `0483:3748`

It controls two daisy-chained pixels using the pulse timing and `GRBW` byte order
verified by the static red/green image. LED1 advances through `R, G, B, W`;
LED2 follows the same cycle one state ahead. Each state is held for three
seconds and repeats continuously. Intensity remains limited to `48/255`.

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

The flash command filters for USB PID `3748` and the detected SWD-DP ID
`0x1ba01477`, so it does not select or modify the H743 board's onboard
ST-Link/V2-1 (`374B`).
