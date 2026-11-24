# Project notes (decision log)

Short, dated entries. Anything that becomes a lasting decision gets an ADR.

## 2026-11-23

- F1 started. D1: MP2 = both nodes (rw612-a, esp32-b) publish directly over Wi-Fi/MQTT; BLE V2V moves to F5.
  D2: gateway = Raspberry Pi 5 + Mosquitto with user/password; home/pit network modes (pit = AP
  "V2V-PIT", broker 10.42.0.1:1883). ADR-0003 rewritten and Accepted. F0 closed.
- End-of-day state (MP2 sprint). F1 done except the real-node dashboard screenshot. F3 done: `rw612-a`
  runs FreeRTOS + lwIP (DEC-012, base `wifi_mqtt`), 3082/3082 msgs in 10 min on home Wi-Fi
  Familia_CM, unstable on the iPhone hotspot. F2 in progress: `esp32-b` publishes but drops on home
  Wi-Fi (~34 % delivered in a clean 10-min test, TCP stalls errno 119, 20 reconnects); cause is
  ESP32-side since the RW612 is perfect on the same network/broker. ESP32 passive buzzer on GPIO33
  (OPEN-013 for node B), brake calibration 170-3010 mV (fault 80/3090). Pi on Familia_CM
  192.168.0.239 (DHCP); pit mode prepared, untested. Next steps in ARQUITECTURA §9.
- DEC-013..015 approved. DEC-013: command contract frozen (closes OPEN-010): `v2v/{node_id}/cmd` and
  `v2v/all/cmd` (`all` reserved), QoS 1, not retained; `alert` and `clear` payloads in
  `interfaces/payload.schema.json` (`$defs/cmd`). DEC-014: the dashboard relays `hard_brake` to the other
  nodes when its toggle is on (remote actuation, not the safety path). DEC-015: MP2 dashboard is Python
  (`dashboard/python/`), Node-RED stays for NXP. Simulator logs/validates cmd; node firmware for cmd pending.
- Contract v0.2: telemetry requires only `node_id`, `ts_ms`, `seq`; added `brake_pct`, `tps_pct`,
  `sensor_ok`; `temp_c` nullable. Simulator publishes two nodes with LWT; broker/credentials from
  args or env vars.

## 2026-11-11

- F0: switched to phase-based workflow. `docs/ARQUITECTURA.md` (Spanish) is now the single
  source of truth (objective, components, decisions, phase table F0–F5). Vision,
  requirements, architecture, `STATUS.md` and `process/workflow.md` archived with `git mv`
  under `docs/archive/layers/` (frozen; REQ/DEC/OPEN/CONFLICT/HIL IDs still valid).
  `CLAUDE.md` updated to the phase workflow.
- ADR-0002, ADR-0004 and ADR-0005 moved to Accepted (already implemented and in use).
  ADR-0003 stays Proposed; it is rewritten in F1 (gateway moves to the Raspberry Pi 5).
- F0 verification: unit tests 47/47 pass (MCUXpresso `make.exe` + `python-zig cc`, see
  `tests/README.md`); integration 6/6 pass including the broker round trip; ESP32 sketch
  compiles (local core 3.3.8); RW612 not built headless (IDE only); CI runs not checked.
- Risks for later phases (no action now):
  - The MCUXpresso project for the RW612 (`firmware/rw612-node/V2V_IOT/`) is git-ignored
    and exists only on Joshua's laptop; how to version it without SDK files is decided in F3.
  - The native Mosquitto on the laptop uses port 1883 (conflicts with
    `gateway/docker-compose.yml`); irrelevant once the Pi is the gateway.

## 2026-11-10

- HANDOFF-006: bench capture `capture3` (FW `ec329fe`, FIR on, CSV on) stored as evidence in
  `docs/04-testing/evidence/pds-2026-11-10/`; HIL-19 passed. DEC-010 confirmed on the bench:
  576 / 600 intervals exactly 50 ms, 12 pairs of 74 / 26 ms (one per DHT11 read), 20.000 Hz,
  fs/2 image gone. DEC-009 parameters (fs 20 Hz, N 9, fc 2 Hz, Hamming) validated, no longer
  ASSUMPTION: useful brake content < ~1.5 Hz; 20–30 dB attenuation above ~3.5 Hz, below −80 dB
  above 5.5 Hz; 2.5 Hz tapping attenuated only ~5 dB (transition band of 9 taps). Trade-off:
  21 taps (500 ms group delay) would be more selective, but low delay on HARD_BRAKE is
  prioritised over selectivity. `APP_LOG_DSP_CSV` back to 0 (normal debug log).
- Bench: the 100 % → 0 % spikes in the first DSP capture were a loose contact (wires and pot
  now fixed in place). No code issue.
- Bench capture 2 (`tools/dsp/out/capture2.csv`, not committed): the brake sample interval
  alternated 62 / 38 ms (mean 50.0 ms). Cause: the blocking HC-SR04 read ran before the pot slot
  and `t_sensor = now` carried its delay forward. The periodic non-uniform sampling put a
  spurious image near fs/2 (≈ 9.6 Hz) in the raw FFT.
- HANDOFF-005 applied:
  - DEC-010: pot slot runs before the HC-SR04 and keeps a fixed 50 ms grid
    (`t_sensor += period`, resync if more than one period late). Host model of the loop: the
    old order reproduces the bench 62 / 38 ms exactly (12 ms echo); with DEC-010 the interval
    is 50 ms outside DHT11 reads (std 0.0–1.0 ms for echo blocks of 5–30 ms; ≈ 5.1 ms with ECHO in
    fault, 35 ms block, because the HC-SR04 phase moves after each DHT11 read), 74 / 26 ms around
    each DHT11 read. With `APP_ENABLE_BRAKE_FIR 0` the only change from the previous firmware is
    DEC-010 plus the brake calibration.
  - DEC-011: divider values as wired (ECHO 2 kΩ / 2.7 kΩ ≈ 2.9 V; brake 5.1 kΩ / 2 kΩ pot /
    220 Ω, 0.10–1.00 V; TPS 2.7 kΩ / 1 kΩ pot / 220 Ω, 0.19–1.03 V; calculated). Brake pot
    calibrated on the bench: raw 2850 / 29320.
  - CONFLICT-003 resolved, option A: filtered brake keeps feeding `alert_evaluate()`;
    HIL-18 uses fast presses to ≥ 90 %; thresholds stay ASSUMPTION (OPEN-005).
    On the bench Joshua reports that HARD_BRAKE does trigger on fast presses with the filter
    on (informal check, not HIL-18 evidence).

## 2026-11-09

- HANDOFF-004 (PDS brake-pressure FIR on top of MP1) applied:
  - REQ-F-015 (FIR as discrete convolution in `firmware/common/`) and REQ-F-016 (UART export
    + host FFT) added.
  - DEC-009: `APP_ENABLE_BRAKE_FIR` (default 1) feeds the filtered brake to `alert_evaluate()`;
    0 = MP1 unchanged (preprocessed translation unit identical to the MP1 commit).
    fs = 20 Hz, N = 9, fc = 2 Hz, Hamming — ASSUMPTION, to validate with a raw capture.
    Group delay 200 ms, logged at boot.
  - `fir_filter.c` + `test_fir_filter.c` (golden test vs `numpy.convolve`); coefficients and
    reference generated by `tools/dsp/design_fir.py`; `tools/dsp/analyze_capture.py` for the
    spectra; `APP_LOG_DSP_CSV` for a clean `dsp,` capture.
  - New: OPEN-016 (faster, timer-triggered ADC sampling). CONFLICT-003 reported: with the
    filter on, fast presses peaking at 75–80 % may no longer meet the HARD_BRAKE rate rule
    (host simulation); HIL-18/HIL-20 will measure it.

## 2026-10-14

- HANDOFF-002 + HANDOFF-003 (MP1 pin map and review fixes) applied:
  - DEC-005 MP1 pin map (D3–D9, Pmod J7-3) in `board_pins.h` and `hardware/pinout.md`;
    OPEN-003 resolved for MP1.
  - DEC-006 HC-SR04 every 100 ms, DHT11 every 2500 ms (datasheet limits enforced at build time).
  - DEC-007 timebase on CTIMER1; CTIMER0 kept for the passive-buzzer PWM on GPIO_0.
  - DEC-008 HC-SR04 out of range = valid 500 cm reading, no echo start = fault;
    `sensor_fault` from the `sensor_ok` bits only.
  - New: OPEN-014 (ADC channel of A0, boot scan), OPEN-015 (5 V tolerance, ECHO divider).
  - Log line gains `brake_raw=` to calibrate `APP_POT_BRAKE_RAW_MIN/MAX` on the bench.

- HANDOFF-001 (MP1 perception node) applied. Approved in chat today:
  - CONFLICT-001 resolved: V2V = BLE; ADR-0001 → Accepted; Thread/802.15.4 stays M4.
  - CONFLICT-002 resolved: MP1 = local part of M3; V2V moves to MP2.
  - REQ-F-014 added (local decide + actuate, no network); REQ-F-009 note: IMU replaced by
    the brake-pressure potentiometer for MP1.
  - DEC-001 `hello_world` base, bare-metal superloop (FreeRTOS deferred to MP2).
  - DEC-002 MP1 scope: HC-SR04, DHT11, 2 potentiometers (brake, TPS), LEDs, buzzer, UART log.
  - DEC-003 arbitration: highest severity, tie-break HARD_BRAKE > OBSTACLE > OVERTEMP.
  - DEC-004 actuator patterns; closes OPEN-006 for MP1.
- New open items: OPEN-012 (`brake_pct`/`tps_pct` in the JSON schema before MP2),
  OPEN-013 (buzzer active vs passive, bench test). OPEN-005 stays open; bench defaults
  in `alert_config_default()`.
- Code: stateful `alert_evaluate()` + `alert_pattern` in `firmware/common` with unit tests;
  RW612 layers `app/services/drivers/board` for MP1; pins pending HANDOFF-002.

## 2026-09-30

- Repository scaffold created (structure, contracts, CI, docs). No firmware logic yet.
- ADR-0001..0005 accepted: BLE for V2V, ESP32/Arduino as node B, local
  Mosquitto, contracts-first monorepo, layered firmware.
- Protocol v0.1: 16-byte `v2v_packet_t`, GATT service UUID base `2b7b0001-fbc4-4f74-84ba-26504f32ca72`.
- Locally installed MCUXpresso SDK for FRDM-RW612 reports version 26.06.00 in its
  manifest; team-wide SDK version → OPEN-001.
- To validate: BLE/802.15.4 simultaneity on the RW612 → OPEN-011 (ASSUMPTION).
- Pending, tracked in [`STATUS.md`](archive/layers/STATUS.md): pin assignment → OPEN-003;
  sensor/actuator and ESP32 dev-kit models → OPEN-004; alert thresholds → OPEN-005;
  time base → OPEN-007.
- Workflow adopted: chat = architect, Claude Code = implementer, rw612-tutor = technical
  advisor; L0–L3 and ADR-0001..0005 moved back to Proposed pending team review.
