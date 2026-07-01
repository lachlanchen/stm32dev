# STM32H743 sensor-head LCD firmware

This app separates measurement from lamp control. It reads AS7343 on I2C address `0x39` and TSL2591 on `0x29` using STM32 I2C1 (`PB8/PB9`), streams CSV on USART1/ST-Link VCP, and plots the latest spectrum/intensity on the attached LTDC screen.

Build:

```powershell
make -C firmware/stm32_sensor_head_lcd
```

Flash:

```powershell
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program firmware/stm32_sensor_head_lcd/build/stm32_sensor_head_lcd.elf verify reset exit"
```

Wiring:

| STM32 | Sensor bus |
|---|---|
| `3.3` | AS7343 VCC, TSL2591 VCC |
| `GND` | AS7343 GND, TSL2591 GND |
| `B8 / PB8` | AS7343 SCL, TSL2591 SCL |
| `B9 / PB9` | AS7343 SDA, TSL2591 SDA |
| open | AS7343 GPIO/INT, TSL2591 INT |

The current AS7343 code uses register-level 18-channel auto-SMUX mode copied from the proven Arduino `as7343_uno_chunked` workflow.
