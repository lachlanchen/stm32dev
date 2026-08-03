# STM32H7 Current Light-Source Pin Map

This note records the physical arrangement currently connected to the STM32H743IIT6 board. Do not infer additional LEDs or interchange the lamp channels.

## Exact topology

| H7 label | MCU pin | Connected device | Interface | Supply |
|---|---|---|---|---|
| A0 | PA0 | 5 V tungsten lamp 1 through its MOS PWM module | PWM control | Independent 5 V lamp supply |
| A1 | PA1 | 5 V tungsten lamp 2 through its MOS PWM module | PWM control | Independent 5 V lamp supply |
| A2 | PA2 | 12 V tungsten lamp through its MOS PWM module | TIM5_CH3 PWM | Independent 12 V lamp supply |
| A3 | PA3 | Exactly one SK6812 RGBW pixel, DI pin | TIM2_CH4 800 kbit/s waveform | Regulated 5 V LED supply |

All controller and module signal grounds must share a common reference. The STM32 GPIO pins are control signals only and must not power any lamp.

## Current A2-only safety test

Firmware target: `flash-tungsten-pa2-smooth-test`

The test uses a 443 Hz carrier and a 16-bit command range:

1. Hold all outputs off for 1 second.
2. Ramp PA2 from the estimated visible threshold `25700/65535` to `65535/65535` over 3 seconds.
3. Ramp PA2 back to `25700/65535` over 3 seconds.
4. Ramp from the threshold to zero over 0.5 seconds.
5. Keep PA2 off indefinitely for cooling.

PA0 and PA1 are explicitly configured as low outputs. PA3 receives an all-zero SK6812 frame and is then actively held low. Therefore this firmware exercises only the 12 V tungsten lamp on A2.

## Integration constraints

- Use TIM5_CH3 for PA2 tungsten PWM.
- Reserve TIM2_CH4 for the 800 kbit/s SK6812 stream on PA3.
- Never assign both signals to TIM2 with incompatible timer periods.
- An SK6812 frame is 32 bits in GRBW byte order. After each frame, PA3 must return to GPIO-low so the data input cannot float.
- Normal idle and every completed experiment must end with all tungsten PWM duties at zero.
