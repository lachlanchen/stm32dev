# STM32Dev Durable Hardware Context

This file is the durable handoff for future STM32 firmware sessions.

## Board

- Geek `GK-STM32H743IIT6` core board with STM32H743IIT6 and RGB LCD.
- External Winbond W9825G6KH SDRAM: 32 MiB at `0xC0000000`.
- Current long-term firmware after hardware diagnostics: Chinese NAND diagnostic GUI.
- Lamp outputs must remain off at idle; PA0/PA1 are forced low by diagnostic images.

## Verified Nonvolatile Hardware

- Internal dual-bank Flash: 2 MiB at `0x08000000`.
- Samsung K9F2G08U0C-SIB0 SLC NAND: 256 MiB plus OOB, FMC Bank 3, verified ID `EC DA 10 95 44`, 96/96 stable reads, status `0xC0`, full read-only diagnostic PASS.
- Winbond W25Q64 QSPI NOR: 8 MiB, verified JEDEC `EF 40 17`, 64/64 stable reads, valid SFDP, status registers and Unique ID readable, read-only diagnostic PASS.
- Do not publish the board-specific W25Q64 Unique ID.

## QSPI Wiring

- `PB2`: QUADSPI clock, AF9.
- `PB6`: QUADSPI Bank 1 chip select, AF10.
- `PF6`: IO3/HOLD, AF9.
- `PF7`: IO2/WP, AF9.
- `PF8`: IO0/MOSI, AF10.
- `PF9`: IO1/MISO, AF10.
- QSPI memory-mapped base is `0x90000000` when configured.

## SD Card State

- A 32 GB SD card has been purchased but has not arrived or been installed.
- Never assume the card is present until the user confirms insertion.
- First contact must be non-destructive: read CID/CSD/OCR, capacity, partition table, and filesystem before any write.

## Safety and Build Rules

- NAND and QSPI diagnostics are read-only unless the user explicitly authorizes storage writes.
- QSPI diagnostic source: `firmware/stm32_sensor_head_lcd/src/qspi_diag_main.c`.
- Build QSPI diagnostic with `make -C firmware/stm32_sensor_head_lcd qspi-diag`.
- Restore the current NAND GUI with `make -C firmware/stm32_sensor_head_lcd nand-diag` followed by programming `build_nand_diag/stm32_nand_diag_lcd.elf`.
- Build and burn every STM32 firmware code change; commit and push relevant repo files.

## Detailed References

- `references/nand-diagnostic-memory-architecture-cn.md`
- `references/w25q64-qspi-detection-cn.md`

## Second STM32 and Addressable LEDs

The second connected board is a verified STM32F103 medium-density target, not
another H7. OpenOCD reports device ID `0x410`, Cortex-M3, 128 KiB Flash, and a
target voltage near 3.215 V. It is reached through the external Telesky ST-Link
V2 with USB VID:PID `0483:3748`; the H7 onboard ST-Link/V2-1 uses `0483:374B`.
Always filter OpenOCD by PID before programming when both boards are attached.

Current verified addressable-LED wiring:

```text
STM32F103 PA0/A0 -> first SK6812RGBW DIN
first SK6812 DOUT -> second SK6812 DIN
external 5 V      -> both LED VCC pins in parallel
external GND      -> both LED GND pins and STM32 GND
```

The missing shared ground caused rapid random flashing. After grounds were
joined, the static red/green firmware worked correctly. Keep 5 V away from the
STM32 3V3 pin. Direct 3.3 V data works on the current short wiring, but a
`74AHCT125` level shifter and 330-ohm data resistor are recommended for a
permanent setup.

F103 firmware images are intentionally independent:

```text
firmware/stm32f103_sk6812_rgbw          rotating color test
firmware/stm32f103_sk6812_static_led1   verified static red/green state
firmware/stm32f103_addressable_led_ramp full 0..255..0 range test
```

Build and burn the ramp without touching the H7:

```powershell
make -C firmware\stm32f103_addressable_led_ramp flash
```
