# STM32 Sensor Head App Scaffold

This folder records the intended STM32 measurement/display app for the H743IIT6 board.

Current status:

- PC cannot see ST-Link or STM32 USB serial yet.
- Firmware is therefore a scaffold/specification, not flashed hardware.
- The expected stream format is implemented by the PC capture script in `scripts/stm32_sensor_serial_capture_plot.py`.

## Wiring

```text
STM32 3.3 -> AS7343 VCC, TSL2591 VCC
STM32 GND -> AS7343 GND, TSL2591 GND
STM32 B8  -> AS7343 SCL, TSL2591 SCL
STM32 B9  -> AS7343 SDA, TSL2591 SDA
INT pins  -> leave open for first test
```

## Runtime design

```text
I2C acquisition ISR/task
-> ring buffer
-> low-rate LCD plot
-> USB CDC/UART raw stream
```

## CSV stream format

```text
us,seq,tsl_ch0,tsl_ch1,tsl_visible,as415,as445,as480,as515,as555,as590,as630,as680,asnir,asclear
```

The PC plot script expects this header.

