# Test plan

## Levels

| Level | What | Tooling | Where | Runs in CI |
|---|---|---|---|---|
| Unit | pure logic in `firmware/common/`, C contract layout | Unity + host gcc | `tests/unit/` | yes |
| Integration | simulator → Mosquitto → subscriber, JSON Schema validation | pytest, paho-mqtt, jsonschema | `tests/integration/` | schema tests yes; broker tests skip without a broker |
| HIL | real boards, BLE link, actuators, timing | checklist + logic analyser/phone scanner | `tests/hil/hil-checklist.md` | no |

## Entry/exit per milestone

- **M1 exit:** HIL-01..HIL-05 pass; latency measured and recorded (even if above target).
- **M2 exit:** integration tests pass against the real broker; HIL-06..HIL-09 pass.
- **M3 exit:** unit tests cover every rule in `alert_evaluate()`; HIL-10..HIL-12 pass.
- **MP1 exit (local part of M3):** `make -C tests/unit` passes; HIL-16..HIL-18 pass on the RW612.

## Traceability

| Requirement | Test(s) | Level | Status |
|---|---|---|---|
| REQ-F-001 | HIL-01 | HIL | not run |
| REQ-F-002 | HIL-02 | HIL | not run |
| REQ-F-003 | UT (packet decode, TBD) · HIL-03 | unit/HIL | not written |
| REQ-F-004 | `test_simulated_telemetry_matches_schema` · `test_simulator_telemetry_reaches_broker_and_is_valid` · HIL-06 | int/HIL | sim: passing |
| REQ-F-005 | IT status/LWT (TBD) · HIL-07 | int/HIL | not written |
| REQ-F-006 | `test_simulated_alert_matches_schema` · HIL-08 | int/HIL | sim: passing |
| REQ-F-007 | HIL-09 (manual, dashboard) | HIL | not run |
| REQ-F-008 | `test_payloads.py` · `test_broker_roundtrip.py` | int | passing / skipped w/o broker |
| REQ-F-009 | HIL-10 | HIL | not run |
| REQ-F-010 | `test_alert_evaluate_*`, `test_obstacle_*`, `test_overtemp_*`, `test_hard_brake_*`, `test_arbitration_*` (DEC-003) · HIL-11 · HIL-16..18 | unit/HIL | unit: passing (bench thresholds, OPEN-005) |
| REQ-F-011 | `test_pattern_*` (DEC-004) · `test_shift_level_*` · HIL-12 | unit/HIL | unit: passing |
| REQ-NF-001 | HIL-04 (latency) | HIL | not run |
| REQ-NF-002 | HIL-06 (rate from broker timestamps) | HIL | not run |
| REQ-NF-003 | HIL-13 (pull AP / stop broker) | HIL | not run |
| REQ-NF-004 | HIL-04 + HIL-06 with both radios active | HIL | not run |
| REQ-NF-005 | HIL-05 (gateway off) | HIL | not run |
| REQ-NF-006 | HIL-14 (BLE reconnect) | HIL | not run |
| REQ-NF-007 | CI job `unit-tests` | unit | passing |
| REQ-NF-008 | `test_protocol_*` · CI `sync_protocol` diff check | unit/CI | passing |
| REQ-NF-009 | `.gitignore` + PR checklist | review | — |
| REQ-NF-010 | HIL-15 | HIL | not run |
| REQ-F-014 | `test_alert_*`, `test_pattern_*` (logic) · HIL-16 obstacle demo · HIL-17 overtemp demo · HIL-18 hard-brake demo | unit/HIL | unit: passing · HIL: not run |

## MP1 HIL demos (RW612, no network)

| ID | Procedure | Expected | Status |
|---|---|---|---|
| HIL-16 | Obstacle demo: move a hand towards the HC-SR04 from > 30 cm to < 15 cm, aim it at open space, then unplug ECHO. | NONE → OBSTACLE WARNING (yellow, 1 Hz beep) → CRITICAL (red 4 Hz + beep); open space → `dist=500.0`, NONE, `fault=0` (DEC-008); ECHO unplugged → `dist=-1.0`, `fault=1`, red/yellow alternating when no other alert. | not run |
| HIL-17 | Overtemp demo: warm the DHT11 (fingers/hair dryer) above 30 °C, then above 35 °C, then let it cool. | OVERTEMP WARNING at ≥ 30 °C, CRITICAL at ≥ 35 °C, stays WARNING until ≤ 28 °C (hysteresis). | not run |
| HIL-18 | Hard-brake demo: turn potentiometer #1 0 → > 70 % fast (< 0.4 s), release; repeat slowly (> 1 s). | Fast: HARD_BRAKE CRITICAL latched ~2 s. Slow: no HARD_BRAKE. | not run |

## How to run

```powershell
make -C tests/unit
pip install -r tests/integration/requirements.txt
pytest tests/integration -v
```
