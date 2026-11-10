# STATUS

Single place for the state of every layer, decision and open question.
Process and status definitions: [`process/workflow.md`](process/workflow.md).

## Active layer

**MP1 (due 2026-10-14)** — Mini Proyecto 1 = local part of M3 (CONFLICT-002): sensors +
`alert_evaluate()` + local actuators, no network (REQ-F-014). Implemented by HANDOFF-001;
pin map applied by HANDOFF-002/003 (DEC-005..008). Remaining: TPS ADC channel (OPEN-014, optional).

**PDS on top of MP1 (2026-11-09)** — brake-pressure FIR (REQ-F-015/016, DEC-009) implemented by
HANDOFF-004: `fir_filter` + unit tests, `tools/dsp/`, app switches. HANDOFF-005 (2026-11-10):
drift-free pot slot (DEC-010), divider values as wired (DEC-011), CONFLICT-003 resolved (option A).
HANDOFF-006 (2026-11-10): HIL-19 passed (`capture3`, [evidence](04-testing/evidence/pds-2026-11-10/)),
DEC-009 parameters validated, `APP_LOG_DSP_CSV` back to 0. HIL-20 pending.

**L0–L3 — team review** continues in parallel (written in the scaffold, 2026-09-30).

## Layer status

| Layer | Status | Missing |
|---|---|---|
| L0 Vision & scope | PROPOSED | team review of [`00-vision/vision.md`](00-vision/vision.md) |
| L1 Requirements | PROPOSED | team review of [`01-requirements/requirements.md`](01-requirements/requirements.md); OPEN-005, OPEN-008, OPEN-009 (OPEN-006 closed for MP1 by DEC-004) |
| L2 Architecture | PROPOSED | team review of [`02-architecture/architecture.md`](02-architecture/architecture.md); ADR-0001 Accepted (CONFLICT-001), ADR-0002..0005 Proposed; OPEN-011 |
| L3 Contracts | PROPOSED | team review of [`../interfaces/`](../interfaces/); OPEN-007, OPEN-010 |
| L4 Hardware | OPEN | MP1 pin map DECIDED (DEC-005), divider values as wired (DEC-011); OPEN-004, OPEN-013, OPEN-014, OPEN-015 |
| L5 Implementation | MP1 in progress | MP1 sources complete with pin map (HANDOFF-001/003); bench build and flash pending; other milestones gated on L0–L3 and L4; OPEN-001, OPEN-002 |
| L6 Verification | MP1 + PDS unit tests passing (`test_alert_logic`, `test_fir_filter`); HIL-19 passed ([evidence](04-testing/evidence/pds-2026-11-10/)) | HIL-16..18 and HIL-20 not run |
| L7 Deliverables | not started | — |

## Decisions (DEC-###)

| ID | Decision | Status | Date | Ref |
|---|---|---|---|---|
| DEC-001 | MP1 base project = SDK `demo_apps/hello_world` for `frdmrw612` (bare-metal superloop). FreeRTOS (future ADR-0006) deferred to MP2. | DECIDED | 2026-10-14 | HANDOFF-001 |
| DEC-002 | MP1 scope: HC-SR04 (distance), DHT11 (temperature), potentiometer #1 = brake-pressure transducer simulator, potentiometer #2 = TPS simulator (optional shift light), LEDs, buzzer(s), UART log. IMU deferred (not sold at Steren). TinyML out of scope. | DECIDED | 2026-10-14 | HANDOFF-001, REQ-F-009 |
| DEC-003 | Severity arbitration: the highest severity wins; tie-break order `HARD_BRAKE > OBSTACLE > OVERTEMP`. | DECIDED | 2026-10-14 | `alert_evaluate()` |
| DEC-004 | Default actuator pattern (closes OPEN-006 for MP1): NONE = green on, buzzer off; WARNING = yellow on, buzzer beep 1 Hz (100 ms on); CRITICAL = red blinking 4 Hz, buzzer beep 4 Hz (on with red); SENSOR FAULT = red+yellow alternating 1 Hz, buzzer off. | DECIDED | 2026-10-14 | `alert_pattern_for()`, REQ-F-011 |
| DEC-005 | MP1 pin map: TRIG GPIO_20 (D7), ECHO GPIO_50 (D8, divider), DHT11 GPIO_52 (D9), brake pot GPIO_46 / `kADC_CH4` (J7-3), LEDs GPIO_15/18/27 (D3/D4/D5, active high), buzzer GPIO_0 (D6, NPN). See [`../hardware/pinout.md`](../hardware/pinout.md). Resolves OPEN-003 for MP1. | DECIDED | 2026-10-14 | HANDOFF-002 |
| DEC-006 | HC-SR04 sampled every 100 ms (datasheet cycle > 60 ms); DHT11 every 2500 ms (datasheet sampling > 2 s); pots stay at 50 ms. Enforced at build time in `main_mp1.c`. | DECIDED | 2026-10-14 | HANDOFF-002 |
| DEC-007 | Timebase = CTIMER1 (SFRO 16 MHz → 1 MHz); CTIMER0 reserved for the passive-buzzer PWM on GPIO_0 (`IO_MUX_CT0_MAT0_OUT`). `kSFRO_to_CTIMER1` verified in `fsl_clock.h`. | DECIDED | 2026-10-14 | HANDOFF-003 |
| DEC-008 | HC-SR04: no rising edge within 5 ms = sensor fault (−1); echo still high after 30 ms = out of range, valid reading (`HCSR04_MAX_CM` = 500 cm). `sensor_fault` in `alert_evaluate()` comes only from the `sensor_ok` bits. | DECIDED | 2026-10-14 | HANDOFF-003 |
| DEC-009 | Brake FIR (PDS): build switch `APP_ENABLE_BRAKE_FIR` (default 1); when on, the filtered value feeds `tlm.brake_pct` and therefore `alert_evaluate()`; when 0, MP1 behaviour is byte-for-byte the same. fs = 20 Hz (existing 50 ms slot). Design N = 9 taps, fc = 2 Hz, Hamming window (regenerate the header with `tools/dsp/design_fir.py`); parameters validated on the bench 2026-11-10 with `capture3` (useful content < ~1.5 Hz; 20–30 dB attenuation above ~3.5 Hz, below −80 dB above 5.5 Hz). Higher N (e.g. 21 taps → 500 ms group delay) not chosen: delay on HARD_BRAKE is prioritised over selectivity; a 2.5 Hz tapping component is attenuated only ~5 dB (transition band of the 9-tap design). Group delay (N−1)/(2·fs) = 200 ms, logged at boot. Evidence: [`04-testing/evidence/pds-2026-11-10/`](04-testing/evidence/pds-2026-11-10/). | DECIDED (parameters validated 2026-11-10) | 2026-11-09 | HANDOFF-004, HANDOFF-006, REQ-F-015, REQ-F-016, HIL-19 |
| DEC-010 | Drift-free pot slot: in `main_mp1.c` the pot/`alert_evaluate()` block runs before the blocking HC-SR04 read in each loop iteration, and its schedule advances by a fixed step (`t_sensor += APP_SENSOR_PERIOD_MS`) instead of `t_sensor = now`. If the loop is more than one period late (e.g. after a DHT11 read) it resynchronises to `now` instead of firing catch-up samples. DEC-006 periods unchanged; the slot is still skipped while a DHT11 read is busy. Fixes the 62 / 38 ms alternation seen on the bench (spurious image near fs/2 in the raw FFT). | DECIDED | 2026-11-10 | HANDOFF-005, DEC-009, HIL-19 |
| DEC-011 | MP1 divider values as wired (replace 2.2 kΩ / 3.3 kΩ and 27 kΩ / 2.7 kΩ; LEDs stay 330 Ω). ECHO → D8: 2 kΩ / 2.7 kΩ, ≈ 2.9 V from 5 V. Brake pot → J7-3: 5.1 kΩ / 2 kΩ pot / 220 Ω, 0.10–1.00 V. TPS pot → A0: 2.7 kΩ / 1 kΩ pot / 220 Ω, 0.19–1.03 V. Voltages calculated, not measured. Brake calibration `APP_POT_BRAKE_RAW_MIN 2850` / `MAX 29320` (bench, 2026-11-10). | DECIDED | 2026-11-10 | HANDOFF-005, [`../hardware/pinout.md`](../hardware/pinout.md), OPEN-015 |

## Open questions (OPEN-###)

| ID | Question | Status | Layer | Owner | Related | Migrated from |
|---|---|---|---|---|---|---|
| OPEN-001 | Team-wide MCUXpresso SDK and IDE versions (local SDK manifest reports 26.06.00). | OPEN | L5 | TBD | [`firmware/rw612-node/README.md`](../firmware/rw612-node/README.md) | project-notes, scaffold report |
| OPEN-002 | Base SDK example for the RW612 project (candidates: `peripheral_ht`, `coex_wifi_peripheral_ht`, `wifi_mqtt`); confirm the coex example imports for FRDM-RW612. | OPEN | L5 | TBD | [`firmware/rw612-node/README.md`](../firmware/rw612-node/README.md) | scaffold report |
| OPEN-003 | Pin assignment for both nodes. Node A MP1 pins resolved by DEC-005; ESP32 and later extras (IMU, haptic, relay, latency probe) still TBD. | OPEN (resolved for MP1) | L4 | TBD | [`hardware/pinout.md`](../hardware/pinout.md) | project-notes, scaffold report |
| OPEN-004 | Exact component models: IMU, ultrasonic, temperature sensor, buzzer (active/passive), haptic motor + driver, relay, level shifter, ESP32 dev kit. | OPEN | L4 | TBD | [`hardware/bom.md`](../hardware/bom.md) | project-notes, scaffold report |
| OPEN-005 | Alert thresholds: g for hard brake, cm for obstacle, °C for overtemp (with the vehicle team). MP1 uses bench defaults in `alert_config_default()` marked `ASSUMPTION (bench default, calibrate — OPEN-005)`. | OPEN | L1 | TBD | REQ-F-010 | requirements, project-notes, scaffold report |
| OPEN-006 | Actuator pattern per severity. | CLOSED for MP1 by DEC-004 | L1 | — | REQ-F-011 | requirements, scaffold report |
| OPEN-007 | Time base of `ts_ms` / `timestamp_ms`: node uptime (current) vs. SNTP-synced epoch; needed for cross-node latency measurements. | OPEN | L3 | TBD | REQ-NF-001, HIL-04 | requirements, interfaces/README, project-notes, scaffold report |
| OPEN-008 | Reconnect time limits: Wi-Fi/MQTT (REQ-NF-003) and BLE (REQ-NF-006). | OPEN | L1 | TBD | HIL-13, HIL-14 | scaffold report |
| OPEN-009 | BLE alert packet delivery ratio target and test distance (REQ-NF-010). | OPEN | L1 | TBD | HIL-15 | scaffold report |
| OPEN-010 | Command set and payloads for `v2v/{node_id}/cmd`. | OPEN | L3 | TBD | [`interfaces/mqtt-topics.md`](../interfaces/mqtt-topics.md) | interfaces/README, scaffold report |
| OPEN-011 | BLE and 802.15.4 cannot run simultaneously on the RW612 — not found in the indexed documentation (needs RW61x datasheet / SDK coexistence docs). | ASSUMPTION | L2 | TBD | ADR-0001 | project-notes |
| OPEN-012 | Add `brake_pct` / `tps_pct` to `interfaces/payload.schema.json` before MP2 (they exist only in `telemetry_t` for now; MP1 has no comms). | OPEN | L3 | TBD | DEC-002 | HANDOFF-001 |
| OPEN-013 | Buzzer type: active vs passive (3 unmarked units with a "+" on top, one marked "TMB12A..."). Pending a bench test. Driver supports both (`BUZZER_KIND` in `app_config.h`, default active = ASSUMPTION). | OPEN | L4 | TBD | [`../hardware/bom.md`](../hardware/bom.md) | HANDOFF-001 |
| OPEN-014 | ADC channel of Arduino A0 (GPIO_42) for the TPS pot — not in the indexed docs. Firmware prints a CH0–CH7 scan at boot (`APP_ADC_SCAN_AT_BOOT 1`). `BOARD_ADC_CH_TPS` stays `#error` behind `APP_ENABLE_SHIFT_LIGHT`. | OPEN | L4 | Joshua | [`../hardware/pinout.md`](../hardware/pinout.md) | HANDOFF-002 |
| OPEN-015 | RW612 GPIO 5 V tolerance — not in the indexed docs (RW61x datasheet missing). Treated as NOT tolerant: HC-SR04 ECHO through a 2 kΩ / 2.7 kΩ divider (≈ 2.9 V calc., DEC-011); whether 2.9 V meets the RW612 input-high threshold is also open here (the HC-SR04 works on the bench). | OPEN | L4 | TBD | [`../hardware/pinout.md`](../hardware/pinout.md) | HANDOFF-002 |
| OPEN-016 | Brake sampling rate for PDS beyond 20 Hz: timer-triggered ADC sampling — ask rw612-tutor for the max ADC rate and a CTIMER/ADC example. | OPEN | L5 | Joshua | DEC-009, REQ-F-015 | HANDOFF-004 |

## Conflicts (CONFLICT-###)

| ID | Description | Status | Ref |
|---|---|---|---|
| CONFLICT-001 | School design (Actividad 3) said V2V over 802.15.4/Thread; repo says BLE. Resolved: **V2V = BLE**; ADR-0001 Accepted; Thread/802.15.4 stays M4 roadmap. | RESOLVED 2026-10-14 | [ADR-0001](adr/0001-ble-for-v2v-instead-of-thread.md) |
| CONFLICT-002 | Course order (MP1 = perception only, no comms) vs repo M1 (V2V first). Resolved: **MP1 = local part of M3** (sensors + `alert_evaluate` + local actuators); V2V moves to MP2. | RESOLVED 2026-10-14 | REQ-F-014, DEC-002 |
| CONFLICT-003 | DEC-009 (FIR output feeds `alert_evaluate()`, default on) vs HIL-18 expectation (any fast press 0 → > 70 % triggers HARD_BRAKE). HARD_BRAKE needs level ≥ 70 % **and** per-sample rate ≥ 150 %/s on the same sample; the low-pass lowers the rate when the level crosses 70 %. Host simulation (N = 9, fc = 2 Hz, 20 Hz): 0 → 75 % in 0.1–0.4 s and 0 → 80 % in 0.2 / 0.4 s trigger raw but **not** filtered; presses to ≥ 90 % (0–0.4 s) trigger both. Resolved, **option A**: the filtered brake keeps feeding `alert_evaluate()` (DEC-009 unchanged); HIL-18 is run with fast presses to **≥ 90 %**. Thresholds stay ASSUMPTION under OPEN-005, to revisit with bench data. | RESOLVED 2026-11-10 | HANDOFF-004, HANDOFF-005, DEC-009, HIL-18, HIL-20, OPEN-005 |
