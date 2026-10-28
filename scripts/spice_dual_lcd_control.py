"""Control only the preserved, separate DLC2 LCD-only diagnostic firmware."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ELF = ROOT/"firmware/stm32_sensor_head_lcd/build_spice_dual_lcd_mcp4728/stm32_spice_dual_lcd_mcp4728.elf"
OPENOCD = r"C:\ProgramData\chocolatey\lib\openocd\tools\install\bin\openocd.exe"
SERIAL = "00710049572D430E4E4C3054"
FIELDS = ("magic ready status count enable topology restart rise_ms amplitude1_mv "
          "amplitude2_mv updates ack_failures late_halves heartbeat cycles phase_ms "
          "carrier_positive").split()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("command", choices=["status", "stop", "opposite-two-boards", "opposite-pairs"])
    p.add_argument("--rise-ms", type=int, default=1000)
    p.add_argument("--confirm-lcd2-on-cd", action="store_true")
    p.add_argument("--save-json", type=Path)
    args = p.parse_args()
    if not 250 <= args.rise_ms <= 10000:
        p.error("--rise-ms must be 250..10000")
    if args.command == "opposite-pairs" and not args.confirm_lcd2_on_cd:
        p.error("Pair topology requires --confirm-lcd2-on-cd after physically wiring LCD2 to C/D")
    symbols = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", "-n", str(ELF)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            try:
                symbols[fields[2]] = int(fields[0], 16)
            except ValueError:
                pass
    def loc(name):
        return f"0x{symbols['dual_'+name]:08x}"
    commands = ["gdb_port disabled", "telnet_port disabled", "tcl_port disabled", "init",
        f'if {{[lindex [read_memory {loc("magic")} 32 1] 0] != 0x444c4332}} {{error "Wrong firmware; no writes permitted"}}']
    if args.command != "status":
        if args.command != "stop":
            commands.extend([
                f'if {{[lindex [read_memory {loc("ready")} 32 1] 0] != 1}} {{error "Diagnostic not ready"}}',
                f'if {{[lindex [read_memory {loc("ack_failures")} 32 1] 0] != 0}} {{error "I2C errors require diagnosis before enabling"}}'])
            needed = "!= 2" if args.command == "opposite-two-boards" else "< 1"
            commands.append(f'if {{[lindex [read_memory {loc("count")} 32 1] 0] {needed}}} {{error "Required distinct DAC addresses are not present"}}')
        commands.append(f'mww {loc("enable")} 0')
        commands.append("sleep 20")
        if args.command != "stop":
            topology = 1 if args.command == "opposite-two-boards" else 2
            for name, value in [("rise_ms", args.rise_ms), ("topology", topology), ("restart", 1), ("enable", 1)]:
                commands.append(f"mww {loc(name)} {value}")
        commands.append("sleep 40")
    commands.extend(f"mdw {loc(name)} 1" for name in FIELDS)
    commands.append(f'mdw {loc("addresses")} 8')
    commands.extend(["mdw 0x58020014 1", "shutdown"])
    cmd = [OPENOCD, "-f", "interface/stlink.cfg", "-c", f"adapter serial {SERIAL}",
           "-f", "target/stm32h7x.cfg", "-c", "adapter speed 1000"]
    for command in commands:
        cmd.extend(["-c", command])
    result = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=30)
    output = result.stdout+"\n"+result.stderr
    if result.returncode:
        raise RuntimeError(output)
    values = {}
    for match in re.finditer(r"0x([0-9a-fA-F]+):\s+((?:[0-9a-fA-F]{8}(?:\s+|$))+)", output):
        base = int(match[1], 16)
        for i, word in enumerate(match[2].split()):
            values[base+4*i] = int(word, 16)
    report = {name: values.get(symbols["dual_"+name]) for name in FIELDS}
    report["addresses"] = [hex(values[symbols["dual_addresses"]+4*i])
                           for i in range(report["count"])]
    report["lamp_pa2_gpio_high"] = bool(values[0x58020014] & (1 << 2))
    report["status_name"] = {0:"boot", 1:"waiting_for_distinct_addresses", 2:"ready_off",
        3:"opposite_loop", 4:"i2c_fault", 5:"bad_topology"}.get(report["status"], "unknown")
    if args.save_json:
        args.save_json.parent.mkdir(parents=True, exist_ok=True)
        args.save_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
