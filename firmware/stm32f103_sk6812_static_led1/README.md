# STM32F103 SK6812 Static LED 1 Test

This non-destructive diagnostic firmware leaves the existing RGBW sequence
firmware unchanged. It continuously drives two daisy-chained SK6812RGBW pixels
without blinking or state changes:

- Pixel 1: red at `48/255`
- Pixel 2: green at `48/255`

Wiring: `PA0/A0 -> LED1 DI`, `LED1 DO -> LED2 DI`, with LED power ground and
STM32 ground connected together.

Build and flash only the external ST-Link target (`0483:3748`):

```powershell
make flash
```
