# H7 PA2 Brief 8 V RMS Overdrive

This independent target briefly overdrives the 6 V tungsten lamp from the measured 12.5 V source. It is intentionally separate from the normal 6 V RMS diagnostic.

```text
D = (8.0 / 12.5)^2 = 0.4096
16-bit duty = 26843 / 65535
off 1.0 s
0 -> 40.96% over 1.5 s
reverse immediately, with no peak hold
40.96% -> 0 over 1.5 s
off indefinitely
```

The 8 V RMS point corresponds to approximately 1.78 times the nominal hot-resistance power of a 6 V lamp. The actual waveform remains 0/12.5 V PWM at 443 Hz. PA0, PA1, and PA3 remain off. The third INA219 channel at `0x42` is sampled once at the turning point, but the no-hold trajectory is the primary safety constraint.

The first run completed and returned PA2 to zero. The single turning-point sample on `0x42` reported 39 mA and 264 mW. Its bus register read 6.776 V, which is phase-aliased and is not continuous lamp voltage. The increase from the 6 V RMS run's roughly 27 mA and 201 mW confirms a higher load response without requiring a peak dwell.
