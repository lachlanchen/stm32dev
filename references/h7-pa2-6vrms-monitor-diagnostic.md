# H7 PA2 6 V RMS Monitor Diagnostic

This independent safety target tests the 6 V tungsten lamp connected to the 12.5 V source through YYNMOS-1 and the Waveshare four-channel INA219 monitor HAT.

## Electrical meaning

The MOS output is not continuous 6 V. It switches between 0 V and approximately 12.5 V. The target duty is:

```text
D = (6.0 / 12.5)^2 = 0.2304
16-bit command = 15099 / 65535
```

This is approximately 6 V RMS for a resistive load. Tungsten resistance changes strongly with temperature, so electrical power remains the better validation quantity.

## Automatic sequence

```text
off 1.0 s
0 -> 23.04% over 1.5 s
sample monitor channels 0x40..0x43 near target
23.04% -> 0 over 1.5 s
off indefinitely
```

PA0, PA1, and the single SK6812 on PA3 remain off. PA2 uses TIM5_CH3 at 443 Hz. PB8/PB9 provide the monitor I2C connection.

Build and flash with:

```powershell
make -C firmware\stm32_sensor_head_lcd flash-tungsten-pa2-6vrms-monitor
```

The firmware caches monitor presence, bus voltage, current, and power in the global `monitor_report`. Read it through ST-Link after `sequence_done` becomes one. The INA219 bus register can remain close to the 12.5 V source or alias with PWM; it must not be interpreted as continuous lamp voltage.

## First measured run

The first run detected all four INA219 addresses and confirmed that the lamp is on the third channel, `0x42`. At the 23.04% target, 50 samples gave approximately 7.268 V from the aliased bus register, 27 mA average shunt-derived current, and 201 mW sample-derived power. Current ranged from 27 to 29 mA and sampled power from 183 to 235 mW. The firmware then set PA2 to zero and remained off.
