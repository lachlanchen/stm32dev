#!/usr/bin/env python3
"""Capture STM32 sensor-head CSV and plot AS7343 spectrum + TSL2591 intensity."""

from __future__ import annotations

import argparse
import csv
import importlib.util
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path


def ensure_module(import_name: str, pip_name: str | None = None) -> None:
    if importlib.util.find_spec(import_name) is None:
        subprocess.check_call([sys.executable, "-m", "pip", "install", pip_name or import_name, "-q"])


def f(row: dict[str, str], key: str, default: float = 0.0) -> float:
    try:
        return float(row.get(key, default))
    except (TypeError, ValueError):
        return default


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="STM32 USB CDC/UART port, e.g. COM7")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=10.0)
    parser.add_argument("--out-dir", default="measurements")
    parser.add_argument("--name", default="stm32_as7343_tsl2591_room_light")
    args = parser.parse_args()

    ensure_module("serial", "pyserial")
    ensure_module("matplotlib")

    import serial
    import matplotlib.pyplot as plt

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = Path(args.out_dir) / f"{args.name}_{stamp}"
    out_dir.mkdir(parents=True, exist_ok=True)
    raw_path = out_dir / "serial_raw.txt"
    csv_path = out_dir / "capture.csv"
    png_path = out_dir / "spectrum_intensity.png"
    summary_path = out_dir / "analysis_summary.md"

    rows: list[dict[str, str]] = []
    header: list[str] | None = None

    with serial.Serial(args.port, args.baud, timeout=1.0) as ser, raw_path.open("w", encoding="utf-8") as raw:
        start = time.time()
        while time.time() - start < args.duration:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            print(line.encode("ascii", errors="replace").decode("ascii"))
            raw.write(line + "\n")
            if line.startswith("#"):
                continue
            if line.startswith("us,"):
                header = line.split(",")
                continue
            if header is None:
                continue
            parts = line.split(",")
            if len(parts) == len(header):
                rows.append(dict(zip(header, parts)))

    if not rows:
        raise RuntimeError("No STM32 sensor CSV rows captured. Check USB serial/ST-Link VCP and firmware stream.")

    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    wavelengths = [
        ("as415", 415),
        ("as445", 445),
        ("as480", 480),
        ("as515", 515),
        ("as555", 555),
        ("as590", 590),
        ("as630", 630),
        ("as680", 680),
    ]
    last = rows[-1]
    xs = [wl for _, wl in wavelengths]
    ys = [f(last, key) for key, _ in wavelengths]
    t0 = f(rows[0], "us")
    ts = [(f(r, "us") - t0) / 1_000_000.0 for r in rows]

    fig, axes = plt.subplots(2, 1, figsize=(12, 8), dpi=160)
    axes[0].plot(xs, ys, marker="o", linewidth=2.0, color="#2563eb", label="latest AS7343 visible spectrum")
    axes[0].set_title("STM32 AS7343 spectrum and TSL2591 intensity")
    axes[0].set_xlabel("nominal wavelength (nm)")
    axes[0].set_ylabel("raw counts")
    axes[0].grid(True, alpha=0.25)
    axes[0].legend()

    axes[1].plot(ts, [f(r, "tsl_visible") for r in rows], color="#16a34a", label="TSL2591 visible = CH0 - CH1")
    axes[1].plot(ts, [f(r, "tsl_ch0") for r in rows], color="#64748b", alpha=0.65, label="TSL CH0")
    axes[1].plot(ts, [f(r, "tsl_ch1") for r in rows], color="#94a3b8", alpha=0.65, label="TSL CH1")
    axes[1].set_xlabel("time (s)")
    axes[1].set_ylabel("raw counts")
    axes[1].grid(True, alpha=0.25)
    axes[1].legend()
    fig.tight_layout()
    fig.savefig(png_path)
    plt.close(fig)

    summary_path.write_text(
        "\n".join([
            "# STM32 AS7343 + TSL2591 room-light capture",
            "",
            f"- Rows: {len(rows)}",
            f"- Capture CSV: `{csv_path.name}`",
            f"- Plot: `{png_path.name}`",
            f"- Last TSL visible: {f(last, 'tsl_visible'):.0f}",
            f"- Last AS7343 visible sum: {sum(ys):.0f}",
            "",
        ]),
        encoding="utf-8",
    )
    print(f"# wrote {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
