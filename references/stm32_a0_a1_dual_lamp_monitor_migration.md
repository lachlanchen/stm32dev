# STM32 A0/A1 Dual-Lamp Monitor Migration

Date: 2026-07-02

## Current hardware goal

Arduino is superseded by the STM32H743 board. The STM32 now handles:

- two MOS/PWM lamp control inputs;
- shared I2C for AS7343, TSL2591, and the Current/Power Monitor HAT;
- LCD visualization;
- serial command control through ST-Link VCP.

## Final wiring

### MOS PWM control

```text
STM32 A0 / PA0 / TIM2_CH1 -> MOS 1 PWM input -> lamp 1
STM32 A1 / PA1 / TIM2_CH2 -> MOS 2 PWM input -> lamp 2
STM32 GND                 -> MOS GND / lamp supply GND
```

The MOS power path stays external:

```text
lamp supply + -> MOS DC+
lamp supply - -> MOS DC-
MOS OUT path  -> monitor current path -> lamp
```

Do not connect lamp power to STM32.

### Shared I2C bus

```text
STM32 B8 / PB8 / SCL -> AS7343 SCL + TSL2591 SCL + Monitor HAT SCL
STM32 B9 / PB9 / SDA -> AS7343 SDA + TSL2591 SDA + Monitor HAT SDA
STM32 3.3V           -> sensor/HAT VCC or 3V3
STM32 GND            -> sensor/HAT GND
```

Expected addresses:

```text
TSL2591     0x29
AS7343      0x39
INA219 ch2  0x40
INA219 ch1  0x41
```

The TCA9548A multiplexer is not required unless the I2C bus becomes unstable or address conflicts appear.

## Firmware behavior

The firmware keeps both lamps off at boot:

```text
PA0 PWM = 0
PA1 PWM = 0
```

Serial commands over ST-Link VCP:

```text
T  run one-shot lamp test:
   lamp1 on for 3 s -> off gap -> lamp2 on for 3 s -> final off

X  immediate all-off
0  immediate all-off
S  toggle display scale mode
```

The LCD keeps the existing spectrum/intensity plot layout. The unused bottom panel now shows:

```text
PWM A0/A1
lamp1 current and power
lamp2 current and power
total electrical power
```

## Important notes

- PA9/A9 and PA10/A10 are reserved by the current firmware for USART1 and should not be used for lamp PWM.
- PA0/A0 and PA1/A1 are used as TIM2 PWM channels.
- The PWM range is 16-bit:

```text
0 ... 65535
```

- The short test uses a fixed safe test duty and ends with both PWM outputs at zero.

## 2026-07-02 verification

The firmware was built and flashed successfully through ST-Link/OpenOCD.

OpenOCD confirmed the STM32 target is reachable. A direct TIM2 register test drove:

```text
PA0/A0 -> lamp1 PWM for 3 s
PA1/A1 -> lamp2 PWM for 3 s
final TIM2 CCR1 = 0
final TIM2 CCR2 = 0
```

RAM status after the test:

```text
AS7343 detected: yes
TSL2591 detected: yes
Monitor 0x41 detected: no
Monitor 0x40 detected: no
```

So the shared I2C bus works for AS7343 and TSL2591, but the monitor HAT was not detected at `0x40` or `0x41` during this verification. Check monitor HAT `3V3`, `GND`, `SDA=PB9`, and `SCL=PB8` before relying on current/power display.
