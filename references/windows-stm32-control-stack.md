# Windows STM32 Control Stack

This note records the Windows tools installed or prepared for controlling the STM32 touch-screen board from `C:\Users\Administrator\Projects\stm32dev`.

## Current Goal

Use Windows to detect, build for, attach to, and eventually program/debug the STM32 board through ST-Link and serial.

The first control mode should be non-destructive:

- Detect USB devices and drivers.
- Attach through OpenOCD over SWD.
- Halt briefly, read registers/flash words, resume.
- Do not erase or flash existing firmware until explicitly requested.

## Installed / Available Tools

Confirmed available:

- `openocd` - SWD/JTAG attach/debug through ST-Link.
- `arm-none-eabi-gcc` - bare-metal Arm compiler.
- `arm-none-eabi-gdb` - debugger client.
- `make` - Makefile-based firmware builds.
- `cmake` - CMake project generation/build support.
- `ninja` - fast build backend.
- `python` - scripting and serial tooling.
- `pyserial` - Python serial port access.
- `platformio` / `pio` - embedded project manager and uploader frontend.
- `stm32pio` - STM32CubeMX + PlatformIO workflow helper.
- `pyocd` - installed, but it did not detect this board because the visible probe is ST-Link rather than CMSIS-DAP.
- `zadig` - GUI driver utility for binding the ST-Link debug interface to WinUSB.
- `UsbDk` - installed USB access helper; did not by itself solve OpenOCD access.

VS Code extensions:

- `ms-vscode.cpptools`
- `marus25.cortex-debug`
- `platformio.platformio-ide`

Attempted but not installed:

- `stm32-for-vscode.stm32-for-vscode` - extension ID not found.
- `stmicroelectronics.stm32-vscode-extension` - install attempt timed out in VS Code CLI.

Official ST tools:

- ST's STM32CubeCLT page states that CubeCLT includes GNU Arm toolchain executables, GDB, STM32CubeProgrammer, SVD files, and programming/debugging support.
- ST's STM32CubeProgrammer page states that CubeProgrammer supports SWD/JTAG through ST-Link and offers GUI and CLI modes.
- ST's STSW-LINK009 page states that the ST-Link USB driver declares ST Debug, Virtual COM, and ST Bridge interfaces for Windows.
- These official ST packages are distributed through ST's `Get Software` web flow, not directly through `winget` or `choco` in this environment.

## Detected Board State

Observed Windows devices:

- `ST-Link Debug`
- `USB Serial Device (COM7)`
- `MBED microcontroller USB Device`
- `USB Composite Device`

Observed USB identity:

- VID/PID: `0483:374B`
- ST-Link identifier: `00710049572D430E4E4C3054`

The serial/VCP side appears as `COM7`. The SWD debug side currently needs a WinUSB/libusb-compatible binding before OpenOCD can control it.

## Current Driver Blocker

OpenOCD failed with:

```text
libusb_open() failed with LIBUSB_ERROR_NOT_FOUND
open failed
```

This means OpenOCD can see that a ST-Link path should exist, but Windows is not exposing the ST-Link debug interface through a libusb-compatible driver.

Use Zadig only on the debug interface:

1. Open Zadig as Administrator.
2. `Options` -> `List All Devices`.
3. Select `ST-Link Debug` / `STM32 STLink`, USB ID `0483:374B`, interface `MI_00`.
4. Select `WinUSB`.
5. Click `Install Driver` or `Replace Driver`.

Do not replace:

- `USB Serial Device (COM7)`
- MBED/mass-storage interface
- Any Arduino serial device

## Helper Scripts

Probe Windows device/tool state:

```powershell
.\scripts\windows_probe_stm32.ps1
```

Non-destructive OpenOCD attach/read/resume:

```powershell
.\scripts\windows_openocd_attach.ps1
```

If SWD speed is unstable:

```powershell
.\scripts\windows_openocd_attach.ps1 -AdapterSpeed 100
```

Listen to the VCP/UART port:

```powershell
.\scripts\windows_serial_listen.ps1 -Port COM7 -Baud 115200 -Seconds 10
```

## Control Policy

- Use `windows_probe_stm32.ps1` before making hardware assumptions.
- Use `windows_openocd_attach.ps1` as the first SWD control test after driver binding.
- Do not flash `firmware/minimal` unless replacing the current touch/demo firmware is intentional.
- Keep the existing firmware safe until the board role is clear.

## Useful References

- STSW-LINK009: https://www.st.com/en/development-tools/stsw-link009.html
- STM32CubeCLT: https://www.st.com/en/development-tools/stm32cubeclt.html
- STM32CubeProgrammer: https://www.st.com/en/development-tools/stm32cubeprog.html
- Zadig: https://zadig.akeo.ie/
