# STM32F103 RGBW Spectral Controller

This firmware is isolated from the previously verified static and ramp images.
It controls two daisy-chained SK6812 RGBW pixels on `PA0/A0` through a fixed
SWD mailbox at `0x20004C00`.

Safety behavior:

- boot state is all channels off;
- an unattended static command expires after two seconds;
- an autonomous scan accepts at most 64 states and 100 repeats;
- every finite scan turns both pixels off when complete;
- invalid commands force all channels off.

The host writes each pixel as packed little-endian `R,G,B,W`. The LED wire
protocol remains the verified `G,R,B,W` order. The external ST-Link is selected
by USB PID `0483:3748`, so the C12880 and H7 probes are not targeted.

```powershell
make -C firmware/stm32f103_rgbw_spectral_controller flash
```
