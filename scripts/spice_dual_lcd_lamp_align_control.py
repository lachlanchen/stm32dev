"""Control only the separate DLA1 dual-LCD and 5 s / 3 s lamp-alignment image."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ELF = ROOT/"firmware/stm32_sensor_head_lcd/build_spice_dual_lcd_lamp_align/stm32_spice_dual_lcd_lamp_align.elf"
OPENOCD = r"C:\ProgramData\chocolatey\lib\openocd\tools\install\bin\openocd.exe"
SERIAL = "00710049572D430E4E4C3054"
FIELDS = ("magic ready status count enable topology restart rise_ms amplitude1_mv "
          "amplitude2_mv updates ack_failures late_halves heartbeat cycles phase_ms "
          "carrier_positive lamp_enable lamp_restart lamp_on lamp_duty "
          "lamp_phase_ms lamp_cycles lamp_target_mv lamp_supply_bound_mv "
          "lamp_on_ms lamp_cool_ms").split()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("command", choices=["status", "stop", "lamp-stop", "align",
                                      "opposite-two-boards", "opposite-pairs"])
    p.add_argument("--rise-ms", type=int, default=1000)
    p.add_argument("--confirm-lcd2-on-cd", action="store_true")
    p.add_argument("--save-json", type=Path)
    args = p.parse_args()
    if not 250 <= args.rise_ms <= 10000:
        p.error("--rise-ms must be 250..10000")
    if args.command in ("align", "opposite-pairs") and not args.confirm_lcd2_on_cd:
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
        f'if {{[lindex [read_memory {loc("magic")} 32 1] 0] != 0x444c4131}} {{error "Wrong firmware; no writes permitted"}}']
    if args.command == "lamp-stop":
        commands.extend([f'mww {loc("lamp_enable")} 0', "sleep 40"])
    elif args.command != "status":
        if args.command != "stop":
            commands.extend([
                f'if {{[lindex [read_memory {loc("ready")} 32 1] 0] != 1}} {{error "Diagnostic not ready"}}',
                f'if {{[lindex [read_memory {loc("ack_failures")} 32 1] 0] != 0}} {{error "I2C errors require diagnosis before enabling"}}'])
            needed = "!= 2" if args.command == "opposite-two-boards" else "< 1"
            commands.append(f'if {{[lindex [read_memory {loc("count")} 32 1] 0] {needed}}} {{error "Required DAC topology is not available"}}')
        commands.append(f'mww {loc("lamp_enable")} 0')
        commands.append(f'mww {loc("enable")} 0')
        commands.append("sleep 20")
        if args.command != "stop":
            topology = 1 if args.command == "opposite-two-boards" else 2
            for name, value in [("rise_ms", args.rise_ms), ("topology", topology), ("restart", 1), ("enable", 1)]:
                commands.append(f"mww {loc(name)} {value}")
            if args.command == "align":
                for name, value in [("lamp_target_mv", 4000), ("lamp_supply_bound_mv", 12500),
                                    ("lamp_on_ms", 5000), ("lamp_cool_ms", 3000),
                                    ("lamp_restart", 1), ("lamp_enable", 1)]:
                    commands.append(f"mww {loc(name)} {value}")
        commands.append("sleep 40")
    commands.extend(f"mdw {loc(name)} 1" for name in FIELDS)
    commands.append(f'mdw {loc("addresses")} 8')
    commands.extend(["mdw 0x40000c3c 1", "mdw 0x40000c2c 1", "shutdown"])
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
    report["tim5_ccr3"] = values[0x40000c3c]
    report["tim5_arr"] = values[0x40000c2c]
    report["lamp_nominal_rms_v_from_command"] = report["lamp_supply_bound_mv"]*.001*(
        report["tim5_ccr3"]/(report["tim5_arr"]+1))**.5
    report["voltage_note"] = "PWM-based nominal RMS using the saved 12.5 V supply bound; not measured lamp voltage."
    report["status_name"] = {0:"boot", 1:"waiting_for_distinct_addresses", 2:"ready_off",
        3:"opposite_loop", 4:"i2c_fault", 5:"bad_topology"}.get(report["status"], "unknown")
    if args.save_json:
        args.save_json.parent.mkdir(parents=True, exist_ok=True)
        args.save_json.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
