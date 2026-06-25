# STM32 Touch Board Session Reference

Date: 2026-06-19
Workspace: `/home/lachlan/ProjectsLFS/STM32`

This records the setup, device observations, installed tools, local files, and verified control state for the connected STM32 touch-screen board.

## Hardware Detected

The board is controlled through an ST-LINK/V2.1 debugger.

```text
USB ID: 0483:374b STMicroelectronics ST-LINK/V2.1
ST-Link serial: 00710049572D430E4E4C3054
ST-Link firmware: V2J45S31 / V2J45M31
Target MCU: STM32H74x_H75x
Chip ID: 0x0450
Flash: 0x200000 bytes / 2 MiB
Flash page size: 0x20000 bytes / 128 KiB
SRAM reported by st-info: 0x20000 bytes / 128 KiB
Observed target voltage: about 3.21-3.25 V
Core detected by OpenOCD: Cortex-M7 r1p1
```

The ST-Link exposes:

```text
/dev/ttyACM0
/dev/serial/by-id/usb-STMicroelectronics_STM32_STLink_00710049572D430E4E4C3054-if02
/dev/stlinkv2-1_* symlinks
```

The Type-C board port appears to power the touch-screen board. It has not appeared as a separate USB device/DFU device during these checks. Keep the board powered from the Type-C port while using SWD.

## Wiring Notes

Use Type-C for board power, and use the 7-pin header for debug/UART signals.

```text
ST-Link GND    -> board GND
ST-Link SWDIO  -> board TMS
ST-Link SWCLK  -> board TCK
ST-Link NRST   -> board RST   optional but useful
ST-Link 3.3V   -> board 3.3V  voltage reference/sense only
```

For UART:

```text
USB-TTL TX -> board RX
USB-TTL RX -> board TX
USB-TTL GND -> board GND
```

Use 3.3 V TTL only. Do not power the screen from a USB-TTL adapter or ST-Link 3.3 V pin unless the board documentation says that current path is safe.

## Installed System Tools

The following Ubuntu packages were installed:

```text
openocd
stlink-tools
dfu-util
picocom
minicom
gdb-multiarch
gcc-arm-none-eabi
binutils-arm-none-eabi
python3-serial
ninja-build
```

Verified tool paths include:

```text
/usr/bin/openocd
/usr/bin/st-info
/usr/bin/st-flash
/usr/bin/dfu-util
/usr/bin/picocom
/usr/bin/minicom
/usr/bin/arm-none-eabi-gcc
/usr/bin/gdb-multiarch
/usr/bin/ninja
```

Local xPack archives and extracted tools were also placed under `tools/`:

```text
tools/downloads/xpack-openocd-0.12.0-7-linux-x64.tar.gz
tools/downloads/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz
tools/xpack-openocd-0.12.0-7/
tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/
```

## Permissions And Auto-Mount Fix

The user `lachlan` was added to:

```text
dialout
plugdev
```

ST-Link and serial permissions were fixed through:

```text
/etc/udev/rules.d/49-stm32-stlink-local.rules
config/49-stm32-stlink-local.rules
```

The rule grants non-root access to ST-Link debug/serial interfaces and tells udisks to ignore the fake MBED mass-storage disk. This stops the desktop from asking for a password when the ST-Link cable is plugged in.

Expected udisks state for the fake ST-Link disk:

```text
HintAuto: false
HintIgnore: true
```

The fake disk may still appear as a block device such as `/dev/sdc`, but it should not auto-mount.

## Project Files Created

Top-level files:

```text
.gitignore
README.md
docs/wiring.md
config/49-stm32-stlink-local.rules
references/stm32-touch-board-session.md
```

Scripts:

```text
scripts/env.sh
scripts/probe.sh
scripts/openocd.sh
scripts/backup_flash.sh
scripts/restore_flash.sh
scripts/flash_bin.sh
scripts/serial_listen.sh
```

Minimal firmware scaffold:

```text
firmware/minimal/Makefile
firmware/minimal/stm32h7_flash.ld
firmware/minimal/src/startup_stm32h7xx.s
firmware/minimal/src/main.c
firmware/minimal/build/minimal.elf
firmware/minimal/build/minimal.bin
```

The minimal firmware builds successfully, but it was not flashed because doing so would overwrite the currently working touch-screen demo.

## Firmware Backups

Two read-only dumps of the current 2 MiB flash were made:

```text
backups/stm32h7_current_flash_2026-06-19.bin
backups/stm32h7_current_flash_20260619_182808.bin
```

Both are byte-identical:

```text
sha256: bbebc7e671a062f1696eaa5e36b6b4c58651f4ad03e7ec0a2af0f16ccb03308e
size: 2097152 bytes
```

Useful strings found in the firmware image:

```text
CTP ID:%s
$LCD ID:%x
?Touch Screen Adjust OK!
Soft Reseting...
XCTP ID:%x
$STM32
TOUCH TEST
Press KEY0 to Adjust
```

The dumped binary is the compiled firmware image, not the original C/C++ source code. The source cannot be recovered exactly from the MCU unless source/debug metadata was stored separately.

## Verified Control

Read-only ST-Link probe:

```sh
st-info --probe
```

Observed result:

```text
Found 1 stlink programmers
version:    V2J45S31
serial:     00710049572D430E4E4C3054
flash:      2097152 (pagesize: 131072)
sram:       131072
chipid:     0x450
dev-type:   STM32H74x_H75x
```

OpenOCD attach was verified:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'targets' \
  -c 'shutdown'
```

Live debug control was verified by halting the CPU, reading PC, reading flash words, and resuming:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'halt' \
  -c 'reg pc' \
  -c 'mdw 0x08000000 8' \
  -c 'resume' \
  -c 'shutdown'
```

Observed debug output included:

```text
[stm32h7x.cpu0] halted due to debug-request
xPSR: 0x81000000 pc: 0x08001340 msp: 0x24000968
0x08000000: 240009c0 08100035 08100049 0810004b 0810064d 0810004f 08100051 00000000
```

This confirms live SWD control. The screen may pause briefly when the CPU is halted.

## Safe Commands

Probe:

```sh
./scripts/probe.sh
```

Start OpenOCD GDB server:

```sh
./scripts/openocd.sh
```

Back up flash:

```sh
./scripts/backup_flash.sh
```

Restore the known working touch demo:

```sh
./scripts/restore_flash.sh backups/stm32h7_current_flash_2026-06-19.bin
```

Listen on ST-Link virtual COM port:

```sh
./scripts/serial_listen.sh
```

Build the minimal test firmware:

```sh
make -C firmware/minimal
```

Flash a binary explicitly:

```sh
./scripts/flash_bin.sh path/to/firmware.bin
```

Do not flash test firmware unless overwriting the current touch demo is intentional.

## Current Practical Status

The host has full debug/programming control through ST-Link/SWD:

- USB debug adapter is visible.
- SWD target probe works.
- OpenOCD can attach.
- CPU halt/resume works.
- Flash read works.
- Flash write works and was used to install the persistent Trio image patch.

The current touch-screen firmware is preserved in `backups/` and can be restored if later experiments overwrite it.

## 2026-06-19 Trio Image Render

Source image:

```text
/home/lachlan/ProjectsLFS/LALACHAN/Trio.png
```

Source properties:

```text
PNG, RGB, 1448x1086
```

The live LTDC framebuffer was identified as:

```text
Framebuffer address: 0xC0000000
Resolution: 1024x600
Pixel format: RGB565
Framebuffer size: 0x12C000 bytes / 1228800 bytes
```

The image was converted to:

```text
build/display/trio_1024x600_rgb565.bin
build/display/trio_1024x600_preview.png
```

Conversion parameters:

```text
Canvas: 1024x600 white background
Resized image: 800x600
Offset: x=112, y=0
Pixel encoding: little-endian RGB565
```

The previous framebuffer was backed up under:

```text
backups/framebuffer/framebuffer_before_trio_*.bin
```

The framebuffer was written with:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'halt' \
  -c 'load_image build/display/trio_1024x600_rgb565.bin 0xC0000000 bin' \
  -c 'resume' \
  -c 'shutdown'
```

Write result:

```text
1228800 bytes written at address 0xC0000000
```

Readback was verified at sample addresses and matched the generated framebuffer data:

```text
0xC0000000: ffffffff
0xC00000E0: f79df79d
0xC0096400: e633e654
0xC006E168: ffbeffbe
0xC006E690: ffbeffbe
0xC0122400: ffbdffbe
```

This render only writes the live framebuffer in external RAM. It does not flash the image into MCU firmware, so a reset or firmware redraw can replace it.

## 2026-06-19 Persistent Trio Image Patch

Goal: make the Trio picture reappear after reset/boot instead of only writing the live framebuffer once.

The original firmware backup was kept unchanged:

```text
backups/stm32h7_current_flash_2026-06-19.bin
sha256: bbebc7e671a062f1696eaa5e36b6b4c58651f4ad03e7ec0a2af0f16ccb03308e
size: 2097152 bytes
```

The generated persistent firmware image is:

```text
build/persistent/stm32h7_touch_trio_persistent_v5.bin
sha256: e36768bf38dfa7df55f3fd7c2180c6f6817f88dfb7ebf553578e2a7ec68ce56c
size: 2097152 bytes
```

The embedded framebuffer asset is:

```text
build/display/trio_1024x600_rgb565.bin
sha256: a5f447078cdac6c6ce7be010f61efb9a655263e7bd90a4b8c8d0290ecb8f2fa3
size: 1228800 bytes
```

The patch stores the RGB565 framebuffer in unused flash gaps:

```text
0x08006000: first segment, 0xFA000 bytes / 1024000 bytes
0x08106000: second segment, 0x32000 bytes / 204800 bytes
```

The code hook is installed at:

```text
0x08005C00
```

The firmware branch patch is installed at:

```text
0x08001320
original bytes: 70 b5 03 46
patched bytes: 04 f0 6e bc
```

The hook recreates the overwritten instructions, checks that LTDC layer 1 points at framebuffer `0xC0000000`, invalidates two D-cache sample lines, samples two framebuffer words from SDRAM-visible content, and copies both flash segments back into SDRAM if the image is not already present. After copying, it cleans the full framebuffer D-cache range so LTDC can see the CPU-written image.

Current guard samples:

```text
0xC008C190: expected 0x6B2C6B4C
0xC00BE5F0: expected 0x316083CC
```

Cache maintenance registers used by the hook:

```text
SCB_DCIMVAC: 0xE000EF5C
SCB_DCCMVAC: 0xE000EF68
```

Source files for the hook:

```text
firmware/persistent_trio/persistent_trio_hook.s
firmware/persistent_trio/persistent_trio_hook.ld
firmware/persistent_trio/branch_to_hook.s
firmware/persistent_trio/branch_to_hook.ld
```

Important corrections:

```text
build/persistent/stm32h7_touch_trio_persistent.bin:
  bad; returned to 0x08001324, not a Thumb return address

build/persistent/stm32h7_touch_trio_persistent_v2.bin:
  booted, but its sample guard could be fooled after the firmware cleared most of the screen white

build/persistent/stm32h7_touch_trio_persistent_v3.bin and v4.bin:
  intermediate cache-diagnosis builds; do not use as the final image

build/persistent/stm32h7_touch_trio_persistent_v5.bin:
  final verified image; use this one
```

The persistent firmware was flashed under reset with:

```sh
st-flash --connect-under-reset write build/persistent/stm32h7_touch_trio_persistent_v5.bin 0x08000000
```

The flash operation completed successfully:

```text
Flash written and verified! jolly good!
```

Post-flash probe:

```text
Found 1 stlink programmers
version:    V2J45S31
serial:     00710049572D430E4E4C3054
flash:      2097152 (pagesize: 131072)
sram:       131072
chipid:     0x450
dev-type:   STM32H74x_H75x
```

Persistence was verified after reset/run by reading framebuffer samples through OpenOCD:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'halt' \
  -c 'reg pc' \
  -c 'mdw 0xC008C190 1' \
  -c 'mdw 0xC00BE5F0 1' \
  -c 'resume' \
  -c 'shutdown'
```

Observed sample values after reboot:

```text
pc: 0x08001340
0xC008C190: 6b2c6b4c
0xC00BE5F0: 316083cc
```

Full framebuffer verification after reset/run and a longer wait:

```sh
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'halt' \
  -c 'dump_image build/display/framebuffer_current_v5_after_long_wait.bin 0xC0000000 0x12C000' \
  -c 'resume' \
  -c 'shutdown'
sha256sum build/display/framebuffer_current_v5_after_long_wait.bin build/display/trio_1024x600_rgb565.bin
```

Observed result:

```text
build/display/framebuffer_current_v5_after_long_wait.bin
sha256: a5f447078cdac6c6ce7be010f61efb9a655263e7bd90a4b8c8d0290ecb8f2fa3

build/display/trio_1024x600_rgb565.bin
sha256: a5f447078cdac6c6ce7be010f61efb9a655263e7bd90a4b8c8d0290ecb8f2fa3

final_compare=byte-identical
```

This confirms that the final persistent image survives reset and the later firmware redraw/white-clear behavior. The CPU was resumed after the final check.

Restore original touch-demo firmware if needed:

```sh
st-flash --connect-under-reset write backups/stm32h7_current_flash_2026-06-19.bin 0x08000000
```
