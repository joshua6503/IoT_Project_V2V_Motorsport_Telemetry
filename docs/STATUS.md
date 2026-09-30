# STATUS

Single place for the state of every layer, decision and open question.
Process and status definitions: [`process/workflow.md`](process/workflow.md).

## Active layer

**L0–L3 — team review.** Written in the scaffold (2026-09-30), pending approval.

## Layer status

| Layer | Status | Missing |
|---|---|---|
| L0 Vision & scope | PROPOSED | team review of [`00-vision/vision.md`](00-vision/vision.md) |
| L1 Requirements | PROPOSED | team review of [`01-requirements/requirements.md`](01-requirements/requirements.md); OPEN-005, OPEN-006, OPEN-008, OPEN-009 |
| L2 Architecture | PROPOSED | team review of [`02-architecture/architecture.md`](02-architecture/architecture.md); ADR-0001..0005 are Proposed; OPEN-011 |
| L3 Contracts | PROPOSED | team review of [`../interfaces/`](../interfaces/); OPEN-007, OPEN-010 |
| L4 Hardware | OPEN | OPEN-003, OPEN-004 |
| L5 Implementation | not started | gated on L0–L3 closed and L4 items per milestone; OPEN-001, OPEN-002 |
| L6 Verification | not started | — |
| L7 Deliverables | not started | — |

## Decisions (DEC-###)

| ID | Decision | Status | Date | Ref |
|---|---|---|---|---|
| — | none yet | | | |

## Open questions (OPEN-###)

| ID | Question | Status | Layer | Owner | Related | Migrated from |
|---|---|---|---|---|---|---|
| OPEN-001 | Team-wide MCUXpresso SDK and IDE versions (local SDK manifest reports 26.06.00). | OPEN | L5 | TBD | [`firmware/rw612-node/README.md`](../firmware/rw612-node/README.md) | project-notes, scaffold report |
| OPEN-002 | Base SDK example for the RW612 project (candidates: `peripheral_ht`, `coex_wifi_peripheral_ht`, `wifi_mqtt`); confirm the coex example imports for FRDM-RW612. | OPEN | L5 | TBD | [`firmware/rw612-node/README.md`](../firmware/rw612-node/README.md) | scaffold report |
| OPEN-003 | Pin assignment for both nodes. | OPEN | L4 | TBD | [`hardware/pinout.md`](../hardware/pinout.md) | project-notes, scaffold report |
| OPEN-004 | Exact component models: IMU, ultrasonic, temperature sensor, buzzer (active/passive), haptic motor + driver, relay, level shifter, ESP32 dev kit. | OPEN | L4 | TBD | [`hardware/bom.md`](../hardware/bom.md) | project-notes, scaffold report |
| OPEN-005 | Alert thresholds: g for hard brake, cm for obstacle, °C for overtemp (with the vehicle team). | OPEN | L1 | TBD | REQ-F-010 | requirements, project-notes, scaffold report |
| OPEN-006 | Actuator pattern per severity. | OPEN | L1 | TBD | REQ-F-011 | requirements, scaffold report |
| OPEN-007 | Time base of `ts_ms` / `timestamp_ms`: node uptime (current) vs. SNTP-synced epoch; needed for cross-node latency measurements. | OPEN | L3 | TBD | REQ-NF-001, HIL-04 | requirements, interfaces/README, project-notes, scaffold report |
| OPEN-008 | Reconnect time limits: Wi-Fi/MQTT (REQ-NF-003) and BLE (REQ-NF-006). | OPEN | L1 | TBD | HIL-13, HIL-14 | scaffold report |
| OPEN-009 | BLE alert packet delivery ratio target and test distance (REQ-NF-010). | OPEN | L1 | TBD | HIL-15 | scaffold report |
| OPEN-010 | Command set and payloads for `v2v/{node_id}/cmd`. | OPEN | L3 | TBD | [`interfaces/mqtt-topics.md`](../interfaces/mqtt-topics.md) | interfaces/README, scaffold report |
| OPEN-011 | BLE and 802.15.4 cannot run simultaneously on the RW612 — not found in the indexed documentation (needs RW61x datasheet / SDK coexistence docs). | ASSUMPTION | L2 | TBD | ADR-0001 | project-notes |

## Conflicts (CONFLICT-###)

| ID | Description | Status | Ref |
|---|---|---|---|
| — | none recorded | | |
