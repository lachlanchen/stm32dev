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

