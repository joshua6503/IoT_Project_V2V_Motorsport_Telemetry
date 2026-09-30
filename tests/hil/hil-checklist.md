# HIL checklist

Record every run: date, commit, firmware versions, who ran it, result, evidence
(photo, log, capture). Copy the table into the PR or into `docs/project-notes.md`.

Setup: node A = FRDM-RW612, node B = ESP32, gateway running for M2+ tests.
Pins per `hardware/pinout.md`.

| ID | Req | Procedure | Pass criterion | Result |
|---|---|---|---|---|
| HIL-01 | REQ-F-001 | Press button on A; sniff with a phone BLE scanner subscribed to Alert | one packet, `alert_type=0x01` | |
| HIL-02 | REQ-F-002 | Press button on A | actuator on B activates | |
| HIL-03 | REQ-F-003 | Replay same `seq`; send packet with major ≠ 0 | B ignores both | |
| HIL-04 | REQ-NF-001, NF-004 | Toggle GPIO on A at detection and on B at actuation; measure with logic analyser, ≥ 50 samples, Wi-Fi active | p95 < 100 ms (target) | |
| HIL-05 | REQ-NF-005 | Power off the gateway / AP, repeat HIL-02 | still works | |
| HIL-06 | REQ-F-004, NF-002 | `mosquitto_sub -t v2v/+/telemetry -v` for 60 s | schema-valid, ≥ 5 msg/s per node | |
| HIL-07 | REQ-F-005 | Reset / unplug a node | `status` goes `offline` (LWT), back to `online` | |
| HIL-08 | REQ-F-006 | Trigger an alert | mirror on `v2v/{id}/alert` | |
| HIL-09 | REQ-F-007 | Open dashboard during HIL-06/08 | live values, alerts, history visible | |
| HIL-10 | REQ-F-009 | Read each sensor against a reference | within TBD tolerance | |
| HIL-11 | REQ-F-010 | Provoke hard brake / obstacle / overtemp | correct alert type raised | |
| HIL-12 | REQ-F-011 | Send each severity to B | correct actuator pattern | |
| HIL-13 | REQ-NF-003 | Turn AP off/on; restart broker | node reconnects within TBD s | |
| HIL-14 | REQ-NF-006 | Move out of range / reset B | BLE reconnects within TBD s | |
| HIL-15 | REQ-NF-010 | 100 alerts at TBD m | delivery ≥ TBD % | |
