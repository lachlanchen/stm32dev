# Active H7 firmware: matched spectral/frame bursts

STM32H743IIT6 currently runs the separately built stm32_spice_spectro_frame application.
Old sources, build directories and original firmware backups remain preserved.

- Source: firmware/stm32_sensor_head_lcd/src/spice_spectro_frame_burst_main.c.
- ELF: firmware/stm32_sensor_head_lcd/build_spice_spectro_frame/stm32_spice_spectro_frame.elf.
- Host: scripts/spice_spectro_frame_control.py.
- ST-Link serial: 00710049572D430E4E4C3054.
- Lamp MOS PWM: PA2. MCP4728 LCD amplitude control: PB8/PB9 I2C.
- C12880 has its own USB firmware, which was not reflashed for this experiment.

This build holds zero-mean bipolar LCD AC while the source is OFF.
Normal requests last at most 2 s with cooling.
Explicit higher-command mode separately limits time above the normal 6 V nominal cap to 200 ms and requires longer cooling.
Voltage labels use a supply assumption; monitor bus readings are not direct lamp RMS measurements.

Use symbols from this ELF and check both application identities.
Do not use another application's addresses or run two OpenOCD/control sessions simultaneously.

The polarizer repository contains the full report:

    publications/spectro_frame_angleA_20261026/V-SPICE_measured_spectral_results.pdf

The primary dataset has 261 unique pairs, repeats/references, 888 selected color frames and 5,939 selected steady spectra.
Higher-command transients are excluded from its inverse model.
The final independent OFF command reported busy=0, fault=0, duty=0 and lcd_mv=0.
