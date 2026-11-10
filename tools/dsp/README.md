# tools/dsp — brake FIR design and capture analysis (PDS)

Host tools for the Digital Signal Processing (PDS) course work on the MP1 node:
REQ-F-015, REQ-F-016, DEC-009.

| File | What |
|---|---|
| `design_fir.py` | Designs the low-pass FIR with the window method (`scipy.signal.firwin`) and writes `firmware/common/include/fir_brake_coeffs.h`. `--reference` also writes the golden test data `tests/unit/fir_reference.h`; `--plot` saves the frequency response to `out/fir_response.png`. |
| `analyze_capture.py` | Reads `dsp,<t_ms>,<x_pct>,<y_pct>` lines from the debug UART (or a file), reports the real sample rate and jitter, plots x[n] / y[n] and their FFT magnitudes, saves PNGs and CSVs to `out/`. |
| `samples/synthetic_capture.txt` | Small synthetic capture (30 s, fs ≈ 20 Hz, slow presses + 6 Hz tone + noise) to try the analyzer without a board. Not real data. |
| `requirements.txt` | numpy, scipy, matplotlib, pyserial |

`.venv/` and `out/` are git-ignored.

## Setup (PowerShell, from the repo root)

```powershell
python -m venv tools\dsp\.venv
tools\dsp\.venv\Scripts\Activate.ps1
pip install -r tools\dsp\requirements.txt
```

## Design / regenerate the coefficients

Current design (DEC-009, **ASSUMPTION — to validate with the FFT of a raw capture**):
fs = 20 Hz (the 50 ms pot slot), N = 9 taps, fc = 2 Hz, Hamming window,
group delay (N−1)/(2·fs) = 200 ms.

```powershell
python tools\dsp\design_fir.py --fs 20 --fc 2 --taps 9 --window hamming --reference --plot
git diff -- firmware/common/include/fir_brake_coeffs.h tests/unit/fir_reference.h
```

The output is deterministic: with the same arguments the diff is empty. Each generated
header carries the command that produced it in its banner. After changing the design,
run the unit tests (`make -C tests/unit`) and rebuild the firmware.

`fc` is the −6 dB point of the windowed design (that is how `firwin` defines it).
`fs` must match `APP_SENSOR_PERIOD_MS` in `firmware/rw612-node/app/app_config.h`;
the firmware logs a `fir WARNING` line at boot if they disagree.

## Capture and analyse

1. In `firmware/rw612-node/app/app_config.h` set `APP_LOG_DSP_CSV 1`
   (`APP_ENABLE_BRAKE_FIR` must be 1), rebuild and flash. The 200 ms log line is
   suppressed; one `dsp,` line is printed per brake sample.
2. Close any serial terminal on the MCU-Link VCOM port, then:

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Description
python tools\dsp\analyze_capture.py --port COM5 --seconds 30
```

Move the brake pot during the capture (slow presses and fast wiggles). The raw lines are
saved to `out/capture_raw.txt`, so the analysis can be repeated offline:

```powershell
python tools\dsp\analyze_capture.py --file tools\dsp\out\capture_raw.txt
python tools\dsp\analyze_capture.py --file tools\dsp\samples\synthetic_capture.txt --tag synthetic
```

Outputs in `tools/dsp/out/` (`<tag>` defaults to `capture`):

| File | Content |
|---|---|
| `<tag>.csv` | t_ms, x_pct, y_pct |
| `<tag>_fft.csv` | f_hz, \|X\|, \|Y\| (one-sided, amplitude-corrected) |
| `<tag>_time.png` | x[n] and y[n] vs time |
| `<tag>_fft.png` | \|X(f)\| and \|Y(f)\| in dB re. the raw peak, fc marked |

The console report gives the mean/median sample rate, dt statistics and the power ratio
y/x above fc.

## Notes and limits

- The FFT assumes uniform sampling at the mean rate. The superloop skips the pot slot
  while a DHT11 read is in progress (DEC-006), so some intervals are longer than 50 ms;
  the jitter report shows how far from uniform the capture is.
- Before DEC-010 the blocking HC-SR04 read ran before the pot slot and the slot rescheduled
  from `now`, so the interval alternated 62 / 38 ms and the raw FFT showed a spurious image near
  fs/2; DEC-010 keeps the slot on a fixed 50 ms grid (expected std < 5 ms outside DHT11 reads).
- Values are printed with one decimal (`fmt1()`, no `%f` on the debug console), i.e.
  0.1 % quantisation of the 0–100 % brake signal.
- Faster, timer-triggered ADC sampling is out of scope for now (OPEN-016).
