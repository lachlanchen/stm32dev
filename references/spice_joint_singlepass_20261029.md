# DCP2 finite continuous source/two-LCD acquisition

This is a separate image. The DCP1 source and build are unchanged.

- Source: `firmware/stm32_sensor_head_lcd/src/spice_joint_continuous_main.c`.
- Build helper: `scripts/build_spice_joint_continuous.ps1`.
- Imported board flags: existing Makefile through `Makefile.joint_continuous`.
- ELF: `firmware/stm32_sensor_head_lcd/build_spice_joint_continuous/stm32_spice_joint_continuous.elf`.
- Runtime magic: `0x44435032` (DCP2), not DCP1's `0x44435031`.
- ST-Link: `00710049572D430E4E4C3054`, H7 PID `0483:374B`.

## Control/safety contract

PA2/TIM5_CH3 controls the lamp at approximately 443 Hz, with 16-bit timer
interpolation. PB8/PB9 drive the existing MCP4728 bus. LCD1 uses A/B and LCD2 C/D;
their shared I2C address intentionally mirrors writes. The 100 Hz bipolar carrier
and complete-pair envelope updates are preserved. Do not apply sustained LCD DC.

The firmware accepts 2--257-point tables, linearly interpolates them, and imposes
a hard 28,000 ms duration limit. Source labels are the established nominal
PWM-equivalent values, capped at 6,000 mV; they are not direct DC/RMS measurements.
Higher-command mode cannot bypass this cap. Five seconds cooling is enforced
after a finite burst/cancellation. Idle, invalid command, I2C fault and watchdog
recovery keep the source off. There is no indefinite source-hold command.

The actual new experiment was one 24-second run with twelve two-second lamp
cycles. The independent LCD envelopes had 6.7 and 9.1 second periods. Runtime
logs reported zero I2C ACK failures, late carrier halves or controller faults.
Final lamp duty, both cell envelopes and busy status were zero. No separate
illuminated pilot/grid/confirmation run was performed.

## Backup and deployment

The complete pre-DCP2 two-MiB flash backup is local at:
`backups/joint_singlepass_20261029_144618/h7_before_singlepass_2MiB.bin`.

SHA256:
`b8d4963d50633c549a6458813ea5594349d98c6cf8c28928610ad726c8ea6e69`.

The matching original DCP1 ELF and `backup.json` are in the same folder. The
first dump exceeded a 15 s socket wait; a fresh transport, complete artifact
length, SHA256 and source-off state were checked before flashing. Allow at least
120 s for a full-flash dump; do not continue issuing mailbox transactions after
a timeout because the old response can desynchronize the TCL stream.

Build/flash:

```powershell
.\scripts\build_spice_joint_continuous.ps1 -Flash
```

The tested board/compiler settings and preserved DCP1 common objects are reused.
The new main object is linked explicitly; the old/default main object is not
linked. GNU Make's inherited recipe override warnings are benign. OpenOCD
reported `Verified OK` and reset the target. Full-flash backup files remain
local, outside committed source. Do not erase NAND, QSPI or SD media.

Restore only when explicitly requested, with the lamp supply made safe:

```powershell
openocd -f interface/stlink.cfg -c "adapter serial 00710049572D430E4E4C3054" -f target/stm32h7x.cfg -c "adapter speed 1000" -c "program backups/joint_singlepass_20261029_144618/h7_before_singlepass_2MiB.bin 0x08000000 verify reset exit"
```

The Windows acquisition/decoding code and raw evidence are in the V-SPICE repo,
`measurements/two_lcd_joint_singlepass_20261029_144618`.
