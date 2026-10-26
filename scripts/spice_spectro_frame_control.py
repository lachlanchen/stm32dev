"""SWD control of the held-LCD, finite spectrometer/frame controller."""
import json
import pathlib
import re
import subprocess
import sys

ROOT=pathlib.Path(r"C:\Users\Administrator\Projects")
sys.path.insert(0,str(ROOT/"polarizer/scripts"))
import spice_short_reference_acquire as base

ELF=ROOT/"stm32dev/firmware/stm32_sensor_head_lcd/build_spice_spectro_frame/stm32_spice_spectro_frame.elf"
base.ELF=ELF; base.control.ELF=ELF
command=base.control.command

def addresses():
    result={}
    for line in subprocess.check_output(["arm-none-eabi-nm",str(ELF)],text=True).splitlines():
        p=line.split()
        if len(p)==3 and re.fullmatch(r"[0-9a-fA-F]+",p[0]): result[p[2]]=int(p[0],16)
    return result

def write_fields(fields):
    symbols=addresses()
    args=["openocd","-f","interface/stlink.cfg","-c",f"adapter serial {base.control.SERIAL}",
          "-f","target/stm32h7x.cfg","-c","adapter speed 1000","-c","init"]
    magic=symbols["ref_burst_magic"]
    args += ["-c",f'if {{[lindex [read_memory 0x{magic:08x} 32 1] 0] != 0x53534631}} {{error "wrong spectro firmware"}}']
    for name,value in fields.items(): args += ["-c",f"mww 0x{symbols[name]:08x} {int(value)}"]
    args += ["-c","shutdown"]
    p=subprocess.run(args,text=True,capture_output=True,timeout=15)
    if p.returncode: raise RuntimeError(p.stdout+p.stderr)

def hold_lcd(millivolts):
    if not 0<=millivolts<=3000: raise ValueError("LCD range is 0..3000 mV")
    state=command("status")
    if state["busy"] or state["fault"]: raise RuntimeError(f"Controller not idle: {state}")
    write_fields({"ref_hold_mv":millivolts,"ref_hold_enable":1})

def set_mode(overdrive=False):
    if command("status")["busy"]: raise RuntimeError("Cannot change limits during a burst")
    write_fields({"ref_overdrive_mode":int(overdrive)})

def off():
    try: write_fields({"ref_hold_enable":0,"ref_hold_mv":0,"ref_overdrive_mode":0})
    finally: state=command("off")
    return state

def snapshot(folder):
    report=base.monitor_snapshot(folder)
    names=addresses(); fields=["ref_burst_magic","ref_hold_enable","ref_hold_mv","ref_overdrive_mode", 
                              "ref_base_cap","ref_high_spent_ms","ref_high_cutoff","ref_peak_duty"]
    args=["openocd","-f","interface/stlink.cfg","-c",f"adapter serial {base.control.SERIAL}",
          "-f","target/stm32h7x.cfg","-c","adapter speed 1000","-c","init"]
    for name in fields: args += ["-c",f"mdw 0x{names[name]:08x} 1"]
    args += ["-c","shutdown"]
    p=subprocess.run(args,text=True,capture_output=True,timeout=15)
    if p.returncode: raise RuntimeError(p.stdout+p.stderr)
    words={int(a,16):int(b,16) for a,b in re.findall(r"0x([0-9a-fA-F]{8}):\s*([0-9a-fA-F]{8})",p.stdout+p.stderr)}
    report.update({name:words[names[name]] for name in fields})
    (folder/"burst_monitor.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    return report

if __name__=="__main__":
    print(json.dumps(off(),indent=2))
