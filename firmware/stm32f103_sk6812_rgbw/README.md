# STM32F103 SK6812RGBW color test

This independent bare-metal image targets the newly detected Cortex-M3 board:

- STM32 device ID: `0x410`
- flash: `128 KiB`
- data output: `PA0 / A0`
- debugger: Telesky ST-Link/V2, USB `0483:3748`

It controls two daisy-chained pixels. LED1 is the first device connected to
`PA0`; LED2 receives LED1's `DO`. The image alternates once per second between
`LED1=red, LED2=green` and `LED1=green, LED2=red`, with both pixels updating on
the same latch edge. Intensity remains limited to `48/255` for bring-up.

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
