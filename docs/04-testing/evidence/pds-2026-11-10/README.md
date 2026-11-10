# PDS bench evidence — 2026-11-10

Bench capture of the brake-pressure FIR on the FRDM-RW612 (REQ-F-015, REQ-F-016).
It validates the DEC-009 design parameters and the DEC-010 sampling fix. Test: HIL-19 in
[`../../test-plan.md`](../../test-plan.md).

## Files

| File | What |
|---|---|
| `capture3_raw.txt` | Raw UART capture: 601 `dsp,<t_ms>,<x_pct>,<y_pct>` lines (x = raw brake %, y = FIR output %). |
| `capture3.csv` | Parsed samples: `t_ms, x_pct, y_pct`. |
| `capture3_time.png` | x[n] and y[n] against time. |
| `capture3_fft.png` | \|X(f)\| and \|Y(f)\| in dB relative to the raw peak, fc marked. |
| `capture2_time.png` | Earlier capture **before DEC-010**: time plot. |
| `capture2_fft.png` | Earlier capture **before DEC-010**: spectrum, with the spurious image near fs/2 (≈ 9.6 Hz) caused by the 62 / 38 ms interval alternation. |

The `capture2` CSV and raw text are not stored here; the two plots show the artefact.

## Setup

- Firmware: commit `ec329fe` (HEAD when this evidence was stored; it includes DEC-010 and the
  DEC-011 brake calibration).
- Build switches in `firmware/rw612-node/app/app_config.h`: `APP_ENABLE_BRAKE_FIR 1`,
  `APP_LOG_DSP_CSV 1`.
- Filter (DEC-009): fs = 20 Hz, N = 9 taps, fc = 2 Hz, Hamming window, group delay 200 ms.
- Wiring: DEC-011 (brake pot 5.1 kΩ / 2 kΩ pot / 220 Ω on J7-3).

## Pot motion protocol (30 s)

| Time | Motion |
|---|---|
| 0–5 s | rest |
| 5–15 s | slow full sweeps |
| 15–25 s | fast tapping |
| 25–30 s | rest |

## Analyzer command

Capture (PowerShell, repo root, serial terminal closed):

```powershell
python tools\dsp\analyze_capture.py --port COMx --seconds 30 --tag capture3
```

Repeat the analysis offline from the stored raw file:

```powershell
python tools\dsp\analyze_capture.py --file docs\04-testing\evidence\pds-2026-11-10\capture3_raw.txt --tag capture3
```

## Results

Sampling (DEC-010):

- 600 intervals: 576 are exactly 50 ms; the other 24 are 12 pairs of 74 / 26 ms (one pair per
  DHT11 read, every 2.5 s). Mean rate 20.000 Hz.
- The fs/2 image seen in `capture2` is gone.

Filter (DEC-009):

- Useful brake content is below ~1.5 Hz.
- Above ~3.5 Hz the filter attenuates 20–30 dB, and the output is below −80 dB above 5.5 Hz.
- The 2.5 Hz component from the fast tapping is attenuated only ~5 dB, because it lies in the
  transition band of a 9-tap design.
- y lags x by 200 ms (cross-correlation peak at 4 samples), matching the design group delay.

Trade-off: a longer filter (e.g. 21 taps → 500 ms group delay) would cut the 2.5 Hz component
more, but it was not chosen. Low delay on HARD_BRAKE detection is prioritised over selectivity,
so the 9-tap design is kept (DEC-009, validated 2026-11-10).
