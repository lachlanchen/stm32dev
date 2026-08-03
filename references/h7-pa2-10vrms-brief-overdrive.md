# H7 PA2 Brief 10 V RMS Overdrive

This independent target briefly drives the 6 V tungsten lamp to a calculated 10 V RMS equivalent from the measured 12.5 V source.

```text
D = (10.0 / 12.5)^2 = 0.64
16-bit duty = 41942 / 65535
off 1.0 s
0 -> 64% over 1.5 s
reverse immediately, with no peak hold
64% -> 0 over 1.5 s
off indefinitely
```

The turning point corresponds to approximately 2.78 times the nominal hot-resistance power of a 6 V lamp. The YYNMOS-1 output remains 0/12.5 V PWM at 443 Hz. PA0, PA1, and PA3 remain off, and the third INA219 channel `0x42` is sampled once at reversal.
## Measured result (2026-08-03)

The programmed trajectory completed and left PA2 at 0% duty. The third
INA219 channel (`0x42`) returned one turning-point sample:

- programmed duty: `41942 / 65535` (`64.0%`)
- assumed supply: `12.5 V`
- equivalent resistive-load voltage: `10.0 V RMS`
- sampled current: `54 mA`
- sampled power: `369 mW`
- sampled bus voltage: `6.840 V` (PWM-aliased instantaneous reading)

The bus-voltage register is not synchronized to the 443 Hz PWM waveform, so
`6.840 V` is not a direct RMS measurement. The current and power sample are
useful only as a brief relative check. Compared with the earlier 8 V trial
(`39 mA`, `264 mW`), the observed load increased as expected. This remains an
overdrive experiment for a nominal 6 V lamp and must not be used as a held
operating point.
