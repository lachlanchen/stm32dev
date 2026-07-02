#!/usr/bin/env python3
"""Control STM32 PA0/PA1 lamp PWM through ST-Link/OpenOCD.

This keeps the STM32 LCD GUI firmware running and only writes TIM2 PWM compare
registers:

  PA0 / TIM2_CH1 / CCR1 -> lamp1
  PA1 / TIM2_CH2 / CCR2 -> lamp2

Safe default: any on command is a timed 3 s pulse, then both channels are set
to zero. Use --hold only when a persistent on state is explicitly needed.
"""

from __future__ import annotations

import argparse
import subprocess
import sys

TIM2_CCR1 = "0x40000034"
TIM2_CCR2 = "0x40000038"
PWM_TOP = 65535
DEFAULT_DUTY = 52428


def openocd(commands: list[str]) -> None:
    cmd = [
        "openocd",
        "-f",
        "interface/stlink.cfg",
        "-f",
        "target/stm32h7x.cfg",
        "-c",
        "adapter speed 1000",
    ]
    for c in commands:
        cmd += ["-c", c]
    subprocess.run(cmd, check=True)


def write_pwm(duty1: int, duty2: int, verify: bool = True) -> None:
    duty1 = max(0, min(PWM_TOP, int(duty1)))
    duty2 = max(0, min(PWM_TOP, int(duty2)))
    commands = [
        "init",
        f"mww {TIM2_CCR1} {duty1}",
        f"mww {TIM2_CCR2} {duty2}",
    ]
    if verify:
        commands += [f"mdw {TIM2_CCR1} 1", f"mdw {TIM2_CCR2} 1"]
    commands += ["shutdown"]
    openocd(commands)


def pulse(duty1: int, duty2: int, seconds: float) -> None:
    ms = max(1, int(seconds * 1000))
    commands = [
        "init",
        f"mww {TIM2_CCR1} {duty1}",
        f"mww {TIM2_CCR2} {duty2}",
        f"sleep {ms}",
        f"mww {TIM2_CCR1} 0",
        f"mww {TIM2_CCR2} 0",
        f"mdw {TIM2_CCR1} 1",
        f"mdw {TIM2_CCR2} 1",
        "shutdown",
    ]
    openocd(commands)


def main() -> int:
    parser = argparse.ArgumentParser(description="STM32 A0/A1 lamp PWM control via OpenOCD")
    parser.add_argument(
        "command",
        choices=["off", "lamp1", "lamp2", "both", "status"],
        help="lamp command; on commands are timed pulses unless --hold is used",
    )
    parser.add_argument("--duty", type=int, default=DEFAULT_DUTY, help="16-bit PWM duty, default 52428")
    parser.add_argument("--seconds", type=float, default=3.0, help="pulse duration, default 3 seconds")
    parser.add_argument("--hold", action="store_true", help="leave selected lamp(s) on instead of pulsing")
    args = parser.parse_args()

    if args.command == "off":
        write_pwm(0, 0)
        return 0
    if args.command == "status":
        openocd(["init", f"mdw {TIM2_CCR1} 1", f"mdw {TIM2_CCR2} 1", "shutdown"])
        return 0

    duty1 = args.duty if args.command in ("lamp1", "both") else 0
    duty2 = args.duty if args.command in ("lamp2", "both") else 0
    if args.hold:
        write_pwm(duty1, duty2)
    else:
        pulse(duty1, duty2, args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
