# STM32 RAM Capture Cache

The STM32 sensor-head firmware keeps a volatile RAM cache of recent AS7343 and TSL2591 samples. This is used when the LCD visualization works but the UART/VCP stream is not available on Windows.

## Why RAM, not internal flash

- RAM writes are fast and safe during high-rate acquisition.
- Internal flash writes require erase/program operations and are not suitable for repeated short captures.
- Flash logging can wear the device and may disturb timing.

## Cache symbols

The firmware exports these symbols for `arm-none-eabi-nm` / OpenOCD memory dumping:

- `capture_log_magic`: expected `0x53504C47`.
- `capture_log_capacity`: number of records.
- `capture_log_record_size`: bytes per record.
- `capture_log_write`: total records written since boot.
- `capture_log`: ring buffer of records.

Each record contains:

- STM32 `t_ms`, display `seq`, AS/TSL sample counters.
- TSL2591 raw `ch0`, raw `ch1`, and `visible = ch0 - ch1`.
- AS7343 `status2`, fixed gain code, and all 18 raw channel registers.
- `ok_flags`: bit 0 AS7343 valid, bit 1 TSL2591 valid, bit 2 display fixed-scale mode.

## Retrieval method

The PC can halt the STM32 through ST-Link/OpenOCD, dump the `capture_log` RAM block, decode it, then resume or reset the target. This does not require UART wiring.

The DualLampHI repo contains a capture script that triggers Arduino D9/D10 lamp modulation, waits for the run, dumps this STM32 RAM cache, decodes it to CSV, and plots the AS7343/TSL data.
