# STM32F103 Addressable LED Ramp

This self-contained firmware tests the full digital intensity range without
overwriting the source of either previously verified firmware image.

Default physical chain:

```text
STM32 PA0/A0 -> SK6812 RGBW pixel 1 DIN
pixel 1 DOUT -> SK6812 RGBW pixel 2 DIN
```

The LED supply and STM32 must share ground. LED power remains an external 5 V
supply. A `330 ohm` series data resistor and a `74AHCT125` level shifter are
recommended for a permanent build.

Behavior:

```text
1 s off
pixel 1 neutral channel: 0 -> 255 -> 0
0.5 s off
pixel 2 neutral channel: 0 -> 255 -> 0
2 s off
repeat
```

For SK6812 RGBW, the neutral channel is W. For WS2812B, neutral uses R+G+B.
Edit `PIXEL_KINDS` in `src/main.c` in physical DIN-to-DOUT order. Mixed chains
are supported because each pixel is encoded with its own 24-bit or 32-bit
frame length.

```powershell
make flash
```

The OpenOCD USB filter `0483:3748` targets only the external ST-Link attached
to the STM32F103 board.
