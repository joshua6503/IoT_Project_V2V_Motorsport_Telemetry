# Requirements

Every value not yet measured is marked **(target, to validate)**. IDs are stable:
never renumber, mark obsolete requirements as *Deleted*.

## Functional

| ID | Requirement | Milestone |
|---|---|---|
| REQ-F-001 | When the test button on a node is pressed, the node shall send a V2V alert of type `manual` over BLE. | M1 |
| REQ-F-002 | On receiving a valid V2V alert, a node shall activate its actuator (LED and/or buzzer at minimum). | M1 |
| REQ-F-003 | A receiver shall discard packets with an unknown protocol major version and duplicates by `(node_id, seq)`. | M1 |
| REQ-F-004 | Each node shall publish telemetry on `v2v/{node_id}/telemetry`, valid against `payload.schema.json`. | M2 |
| REQ-F-005 | Each node shall publish `online` (retained) on `v2v/{node_id}/status` on connect and register `offline` as its LWT. | M2 |
| REQ-F-006 | Each node shall mirror every V2V alert it sends on `v2v/{node_id}/alert`. | M2 |
| REQ-F-007 | The dashboard shall show live telemetry, alerts and online state per node, and keep a history. | M2 |
| REQ-F-008 | The simulator shall publish schema-valid telemetry and alerts with configurable broker, node id and rate. | M2 |
| REQ-F-009 | The RW612 node shall read an IMU, a distance sensor and a temperature sensor (models TBD). | M3 |
| REQ-F-010 | The node shall detect `hard_brake`, `obstacle` and `overtemp` locally with `alert_evaluate()` (thresholds TBD). | M3 |
| REQ-F-011 | The receiver shall map severity to actuator pattern (mapping TBD): LED/shift light, buzzer, haptic, relay. | M3 |
| REQ-F-012 | Nodes shall form an 802.15.4/Thread mesh with a border router. | M4 (roadmap) |
| REQ-F-013 | Telemetry history shall be stored in InfluxDB and shown in Grafana. | M4 (roadmap) |

## Non-functional

| ID | Requirement | Milestone |
|---|---|---|
| REQ-NF-001 | V2V alert latency, from detection on node A to actuator on node B, < 100 ms (target, to validate). | M1 |
| REQ-NF-002 | Telemetry publish rate ≥ 5 Hz per node (target, to validate). | M2 |
| REQ-NF-003 | Nodes shall reconnect automatically to Wi-Fi and to the MQTT broker after a loss, within TBD s (target, to validate). | M2 |
| REQ-NF-004 | On the RW612, BLE (V2V) and Wi-Fi (MQTT) shall operate concurrently without breaking REQ-NF-001/002 (target, to validate). | M2 |
| REQ-NF-005 | The V2V path shall not depend on Wi-Fi or the broker: alerts work with the gateway powered off. | M1 |
| REQ-NF-006 | BLE link shall reconnect automatically after a disconnection, within TBD s (target, to validate). | M1 |
| REQ-NF-007 | Hardware-independent logic shall live in `firmware/common/` and have host unit tests run in CI. | all |
| REQ-NF-008 | Protocol contracts shall be versioned in `interfaces/`; both firmwares shall build from the same contract version. | all |
| REQ-NF-009 | No secrets (Wi-Fi credentials, keys) and no NXP SDK files shall be committed. | all |
| REQ-NF-010 | Alert packet delivery ratio over BLE ≥ TBD % at TBD m (target, to validate). | M3 |

## Open questions

Tracked in [`../STATUS.md`](../STATUS.md):

- Alert thresholds → OPEN-005
- Time base → OPEN-007
- Actuator patterns per severity → OPEN-006
