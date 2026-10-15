# Project notes (decision log)

Short, dated entries. Anything that becomes a lasting decision gets an ADR.

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
- Pending, tracked in [`STATUS.md`](STATUS.md): pin assignment → OPEN-003;
  sensor/actuator and ESP32 dev-kit models → OPEN-004; alert thresholds → OPEN-005;
  time base → OPEN-007.
- Workflow adopted: chat = architect, Claude Code = implementer, rw612-tutor = technical
  advisor; L0–L3 and ADR-0001..0005 moved back to Proposed pending team review.
