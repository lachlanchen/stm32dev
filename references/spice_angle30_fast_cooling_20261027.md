# V-SPICE analyzer-30 acquisition controller

The prior controller and all working sources remain unchanged.
The new source is firmware/stm32_sensor_head_lcd/src/spice_spectro_frame_fast_main.c.
It is a separate copy of the proven spectrometer/frame finite-burst controller.

The current board flash was read in full, without writing NAND/QSPI, into
backups/spice_angle30_fast_20261027/original_flash_2MiB.bin. The previous exact
ELF and BIN were copied there as well. These local recovery files are not
published. The new build was programmed and verified on the H7.

## Preserved controls

- PA2: tungsten MOS PWM, unchanged timer/pin and 16-bit duty table.
- LCD: existing MCP4728 differential AC drive, unchanged 5 ms polarity halves.
- Normal lamp command bound: the original conservative nominal 6 V RMS cap.
- Normal burst duration: at most 2 s, with watchdog and boot/off interlocks.
- INA219 monitor channel and bus/current checks remain unchanged.
- No overdrive acquisition is included in the new angle series.

## Cooling

The old hard-coded normal cooldown was 10 s. The new volatile ref_cooldown_ms
defaults to 5000 and is clamped by firmware to 3000..15000 ms. The user explicitly
requested 3000 ms for this measurement. The acquisition host sets that value.
High-mode cooldown remains 15 s, although high mode is disabled by this host.
LCD settling and illumination plateau selection are not shortened.

## Build

From the repository root:

    make -C firmware/stm32_sensor_head_lcd PROJECT=stm32_spice_spectro_frame_fast BUILD_DIR=build_spice_spectro_frame_fast APP_SRCS=src/spice_spectro_frame_fast_main.c -j4

The binary remains separate from every previous target.

## Host transport

polarizer/scripts/spice_persistent_swd.py uses one localhost-only OpenOCD server
instead of repeatedly launching a new process for each mailbox read. It checks
both existing mailbox identities plus ref_fast_cool_magic=0x46435331. Target
execution is not halted or reset during acquisition.

Two bring-up issues were fixed with the user's approval:

- The linker removed an otherwise unused version flag. The startup integrity
  check now references it, retaining both the flag and the runtime check.
- A Windows command wrapper left an OpenOCD child alive. The transport now
  launches the actual executable and shuts it down through TCL before fallback
  process cleanup.

At acquisition completion or an exception, the host disables the lamp and
parks the LCD at zero. Firmware independently bounds every illumination burst.
