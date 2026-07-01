# STM32 sensor-head status

Date: 2026-07-01

## Goal

Separate lamp control from measurement:

- Arduino remains the lamp controller.
- STM32H743IIT6 core board becomes the measurement/display head.
- AS7343 measures spectrum.
- TSL2591 measures total intensity.
- Screen plotting must not block acquisition.

## Current hardware detection

Windows now sees the STM32/ST-Link path:

- `COM7` USB Serial Device from `VID_0483&PID_374B&MI_02`
- `ST-Link Debug` from `VID_0483&PID_374B&MI_00`
- `MBED microcontroller USB Device`

OpenOCD can attach over SWD:

```text
STLINK V2J45M31
Target voltage: about 3.24 V
Cortex-M7 detected
PC: 0x08001340
```

The MCU is reachable and flash is readable. However, `COM7` currently produces no bytes at `115200`, `57600`, or `9600` baud, so the firmware currently on the board is not streaming AS7343/TSL2591 sensor frames yet.

Next blocking step: flash a measurement firmware that initializes I2C1 on `PB8/PB9`, reads AS7343 and TSL2591, and streams CSV frames over the ST-Link virtual COM port.

## First wiring

Use a shared 3.3 V I2C bus:

| STM32 silk | Function | AS7343 | TSL2591 |
|---|---|---|---|
| `3.3` | 3.3 V | `VCC` / `VIN` | `VCC` / `VIN` |
| `GND` | ground | `GND` | `GND` |
| `B8` | `PB8 / I2C1_SCL` | `SCL` | `SCL` |
| `B9` | `PB9 / I2C1_SDA` | `SDA` | `SDA` |
| no-connect first | interrupt later | `INT/GPIO` open | `INT` open |

Do not leave the same sensors connected to Arduino `SDA/SCL` and STM32 `SDA/SCL` at the same time. Use only one I2C master.

## Expected I2C addresses

- AS7343: `0x39`
- TSL2591: `0x29`

## Startup image

The only existing candidate image found in this repo was:

```text
docs/assets/stm32h7-trio-display.jpg
```

It has been copied as the current startup placeholder:

```text
docs/assets/aya-lala-sasa-startup.jpg
```

If there is a different original Ayachan / aya-lala-sasa image, place it in `docs/assets/` and replace this file.

## Measurement architecture

Use acquisition-first design:

1. A timer timestamps samples.
2. The I2C acquisition task reads AS7343/TSL2591 into a RAM ring buffer.
3. The display task reads the latest buffer state at lower refresh rate.
4. USB CDC/UART streams raw frames to the PC.

The display must never run inside the high-rate sensor read loop.

## Speed limit

TSL2591 minimum normal integration is 100 ms, so it cannot verify true sub-100 ms modulation. AS7343 can be configured faster, but full multi-channel spectral frames are still limited by integration/readout. For sub-100 ms intensity verification, use a photodiode + transimpedance amplifier + STM32 ADC/DMA.
