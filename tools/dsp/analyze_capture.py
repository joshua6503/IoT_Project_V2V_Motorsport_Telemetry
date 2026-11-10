#!/usr/bin/env python3
"""Compare the raw and FIR-filtered brake signal of the RW612 node (REQ-F-016, DEC-009).

Reads `dsp,<t_ms>,<x_pct>,<y_pct>` lines (firmware built with APP_LOG_DSP_CSV 1) from the
debug UART or from a saved text file, then:
  - reports the actual sample rate and timing jitter from t_ms,
  - plots x[n] and y[n] over time,
  - plots the one-sided FFT magnitude of both (mean removed, Hann window, Hz axis),
    with fc marked,
  - saves PNGs and CSVs to tools/dsp/out/.

Non-`dsp,` lines (boot messages, adc_scan, ...) are ignored.

Usage (PowerShell, from the repo root, venv active):
    python tools/dsp/analyze_capture.py --port COM5 --seconds 30
    python tools/dsp/analyze_capture.py --file tools/dsp/samples/synthetic_capture.txt
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "tools" / "dsp" / "out"
COEFFS_H = ROOT / "firmware" / "common" / "include" / "fir_brake_coeffs.h"

COLOR_X = "#2a78d6"  # raw x[n]      (dataviz categorical slot 1)
COLOR_Y = "#eb6834"  # filtered y[n] (dataviz categorical slot 2)
COLOR_MUTED = "#6b6a63"
COLOR_GRID = "#e4e3dd"


def default_fc() -> float | None:
    """FIR_BRAKE_FC_HZ from the generated header, if present."""
    try:
        text = COEFFS_H.read_text(encoding="utf-8")
    except OSError:
        return None
    m = re.search(r"#define\s+FIR_BRAKE_FC_HZ\s+([0-9.eE+-]+)f?", text)
    return float(m.group(1)) if m else None


def parse_lines(lines) -> np.ndarray:
    """Rows of (t_ms, x, y) from `dsp,` lines; malformed lines are skipped."""
    rows = []
    for line in lines:
        line = line.strip()
        if not line.startswith("dsp,"):
            continue
        parts = line.split(",")
        if len(parts) != 4:
            continue
        try:
            rows.append((float(parts[1]), float(parts[2]), float(parts[3])))
        except ValueError:
            continue
    return np.array(rows, dtype=np.float64).reshape(-1, 3)


def capture_serial(port: str, baud: int, seconds: float, raw_path: Path) -> list[str]:
    import serial  # pyserial

    lines: list[str] = []
    deadline = time.monotonic() + seconds
    with serial.Serial(port, baud, timeout=0.5) as ser:
        ser.reset_input_buffer()
        print(f"capturing {seconds:g} s from {port} @ {baud} ...")
        while time.monotonic() < deadline:
            raw = ser.readline()
            if raw:
                lines.append(raw.decode("ascii", errors="replace").rstrip("\r\n"))
    raw_path.parent.mkdir(parents=True, exist_ok=True)
    raw_path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {raw_path.relative_to(ROOT).as_posix()} ({len(lines)} lines)")
    return lines


def timing_report(t_ms: np.ndarray) -> tuple[float, dict]:
    dt = np.diff(t_ms)
    if dt.size == 0 or np.any(dt <= 0):
        bad = int(np.sum(dt <= 0))
        if dt.size == 0 or bad == dt.size:
            raise SystemExit("error: t_ms is not increasing; cannot estimate fs")
        print(f"warning: {bad} non-increasing timestamps ignored in the timing stats")
        dt = dt[dt > 0]
    fs = 1000.0 / float(np.mean(dt))
    stats = {
        "samples": int(t_ms.size),
        "duration_s": float((t_ms[-1] - t_ms[0]) / 1000.0),
        "fs_mean_hz": fs,
        "fs_median_hz": 1000.0 / float(np.median(dt)),
        "dt_mean_ms": float(np.mean(dt)),
        "dt_std_ms": float(np.std(dt)),
        "dt_min_ms": float(np.min(dt)),
        "dt_max_ms": float(np.max(dt)),
        "jitter_pp_ms": float(np.max(dt) - np.min(dt)),
    }
    return fs, stats


def spectrum(v: np.ndarray, fs: float) -> tuple[np.ndarray, np.ndarray]:
    """One-sided FFT magnitude: mean removed, Hann window, amplitude-corrected."""
    n = v.size
    w = np.hanning(n)
    V = np.fft.rfft((v - np.mean(v)) * w)
    mag = np.abs(V) * 2.0 / np.sum(w)
    f = np.fft.rfftfreq(n, d=1.0 / fs)
    return f, mag


def to_db(mag: np.ndarray, ref: float) -> np.ndarray:
    return 20 * np.log10(np.maximum(mag, 1e-12) / ref)


def style(ax) -> None:
    ax.grid(True, color=COLOR_GRID, linewidth=0.8)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)


def plot_all(t_s, x, y, f, X, Y, fc, fs, out_dir: Path, tag: str) -> list[Path]:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    paths = []

    fig, ax = plt.subplots(figsize=(10, 4.5), dpi=120)
    ax.plot(t_s, x, color=COLOR_X, linewidth=1.2, label="x[n] raw")
    ax.plot(t_s, y, color=COLOR_Y, linewidth=1.5, label="y[n] FIR")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Brake (%)")
    ax.set_title("Brake pressure: raw vs FIR-filtered", loc="left")
    ax.legend(loc="lower right", bbox_to_anchor=(1.0, 1.0), ncol=2, frameon=False)
    style(ax)
    fig.tight_layout()
    p = out_dir / f"{tag}_time.png"
    fig.savefig(p)
    plt.close(fig)
    paths.append(p)

    ref = max(float(np.max(X)), 1e-12)
    fig, ax = plt.subplots(figsize=(10, 4.5), dpi=120)
    ax.plot(f, to_db(X, ref), color=COLOR_X, linewidth=1.2, label="|X(f)| raw")
    ax.plot(f, to_db(Y, ref), color=COLOR_Y, linewidth=1.5, label="|Y(f)| FIR")
    if fc is not None:
        ax.axvline(fc, color=COLOR_MUTED, linestyle="--", linewidth=1)
        ax.annotate(f"fc = {fc:g} Hz", (fc, 0), xytext=(4, -14), textcoords="offset points",
                    color=COLOR_MUTED, fontsize=9)
    ax.set_xlim(0, fs / 2)
    ax.set_ylim(-80, 5)
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB re. max of raw)")
    ax.set_title(f"FFT magnitude (Hann, mean removed), fs = {fs:.2f} Hz", loc="left")
    ax.legend(loc="lower right", bbox_to_anchor=(1.0, 1.0), ncol=2, frameon=False)
    style(ax)
    fig.tight_layout()
    p = out_dir / f"{tag}_fft.png"
    fig.savefig(p)
    plt.close(fig)
    paths.append(p)
    return paths


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    src = p.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", help="serial port, e.g. COM5")
    src.add_argument("--file", type=Path, help="text capture with dsp, lines")
    p.add_argument("--seconds", type=float, default=30.0, help="capture length with --port (default 30)")
    p.add_argument("--baud", type=int, default=115200, help="debug UART baud (default 115200)")
    p.add_argument("--fc", type=float, default=None,
                   help="cutoff to mark in Hz (default: FIR_BRAKE_FC_HZ from fir_brake_coeffs.h)")
    p.add_argument("--tag", default="capture", help="output file prefix in tools/dsp/out/ (default capture)")
    p.add_argument("--no-plot", action="store_true", help="skip the PNGs (CSV and report only)")
    args = p.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    if args.port:
        lines = capture_serial(args.port, args.baud, args.seconds, OUT_DIR / f"{args.tag}_raw.txt")
    else:
        lines = args.file.read_text(encoding="utf-8", errors="replace").splitlines()

    data = parse_lines(lines)
    if data.shape[0] < 16:
        print(f"error: only {data.shape[0]} dsp, lines found (need >= 16). "
              "Is the firmware built with APP_LOG_DSP_CSV 1?", file=sys.stderr)
        return 1

    t_ms, x, y = data[:, 0], data[:, 1], data[:, 2]
    fs, st = timing_report(t_ms)
    fc = args.fc if args.fc is not None else default_fc()

    print(f"samples      {st['samples']}  ({st['duration_s']:.2f} s)")
    print(f"fs           mean {st['fs_mean_hz']:.3f} Hz, median {st['fs_median_hz']:.3f} Hz")
    print(f"dt (ms)      mean {st['dt_mean_ms']:.2f}  std {st['dt_std_ms']:.2f}  "
          f"min {st['dt_min_ms']:.0f}  max {st['dt_max_ms']:.0f}  (jitter p-p {st['jitter_pp_ms']:.0f} ms)")

    # FFT assumes uniform sampling at the mean rate; jitter above shows how true that is.
    f, X = spectrum(x, fs)
    _, Y = spectrum(y, fs)

    if fc is not None:
        hi = f > fc
        if np.any(hi):
            px, py = float(np.sum(X[hi] ** 2)), float(np.sum(Y[hi] ** 2))
            if px > 0:
                print(f"power above fc={fc:g} Hz: y/x = {10 * np.log10(max(py, 1e-24) / px):.1f} dB")

    with open(OUT_DIR / f"{args.tag}.csv", "w", encoding="utf-8", newline="\n") as fh:
        fh.write("t_ms,x_pct,y_pct\n")
        for row in data:
            fh.write(f"{row[0]:.0f},{row[1]:g},{row[2]:g}\n")
    with open(OUT_DIR / f"{args.tag}_fft.csv", "w", encoding="utf-8", newline="\n") as fh:
        fh.write("f_hz,X_mag,Y_mag\n")
        for fi, xi, yi in zip(f, X, Y):
            fh.write(f"{fi:.6g},{xi:.6g},{yi:.6g}\n")
    written = [OUT_DIR / f"{args.tag}.csv", OUT_DIR / f"{args.tag}_fft.csv"]

    if not args.no_plot:
        t_s = (t_ms - t_ms[0]) / 1000.0
        written += plot_all(t_s, x, y, f, X, Y, fc, fs, OUT_DIR, args.tag)

    for w in written:
        print(f"wrote {w.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
