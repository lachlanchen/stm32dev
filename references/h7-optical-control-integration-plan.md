# H7 Optical Control Integration Plan

## Current verified outputs

- `PA3/A3`: addressable-pixel data using TIM2_CH4 at 800 kbit/s.
- SK6812RGBW uses 32-bit `GRBW`; preserved targets provide 3-second and 1 kHz demos.
- WS2812B uses 24-bit `GRB`; its 1 kHz demo is a separate target.
- PA3 must become push-pull GPIO output-low after every frame and remain low until the next frame.
- `PA2/A2`: reserved for the 12 V tungsten-lamp MOS PWM test; not changed by the pixel demos.

## Verified addressable-pixel lessons

- SK6812RGBW requires 32-bit `GRBW`; WS2812B requires 24-bit `GRB`.
- Both devices run smoothly at 1,000 command frames/s with TIM2_CH4 on PA3.
- The initial H7 flashing fault occurred because PA3 remained in alternate-function mode after CH4 was disabled. The working implementation changes PA3 to push-pull output-low after every frame and restores AF1 only for transmission.
- Keep the preserved 3-second SK6812 test as the hardware baseline before integration.

## Timer allocation for parallel control

- `TIM2_CH4 / PA3`: addressable-pixel waveform at 800 kbit/s.
- `TIM5_CH3 / PA2`: tungsten MOS PWM at 443 Hz.

PA2 also supports TIM2_CH3, but using it would force the tungsten lamp and PA3
pixel stream to share one timer period, which is impossible because their
carrier rates differ by roughly three orders of magnitude. TIM5 avoids that
conflict and permits parallel real-time control.

The independent tungsten test maps the prior visible-start estimate
`100/255` to `25700/65535`, ramps to full duty over three seconds, ramps back
over three seconds, fades fully off in 0.5 seconds, and then remains off for
unlimited cooling. This single-run behavior is the required safety default.

## Current measurement topology

The first 4f system merges the illumination sources. A second 4f system splits
the merged output into two measurement branches:

- branch 1: C12880 spectrometer;
- branch 2: currently TSL2591, later potentially replaced by the event camera.

TSL2591 is currently connected to the H7 I2C bus at `PB8=SCL`, `PB9=SDA`.
AS7343 is currently unplugged. Either sensor must be treated as optional and
detected at runtime rather than required during startup.

## Future unified firmware

The integrated application should retain the existing LCD visualization and
add non-blocking modules for the power monitor, optional TSL2591/AS7343,
C12880 synchronization, addressable RGB/RGBW selection, and smooth PA2 tungsten
PWM. Control and acquisition need timestamped schedules rather than blocking
delays so lamp, pixel, sensor, and event-camera triggers remain aligned.

Do not merge these modules until each simple hardware test is independently
verified. The next test is smooth 12 V tungsten control on `PA2/A2`.
