# ADR-0001: Use BLE for V2V instead of Thread/802.15.4

- **Status:** Proposed
- **Date:** 2026-09-30
- **Deciders:** J. A. Chávez Montero, M. A. Mireles González, K. Rincones Partida, J. Santana Renteria

## Context

Main argument — the hardware we have cannot form a mesh:

- Only **one** RW612 is available.
- A Thread mesh needs **at least 3 802.15.4 nodes plus a border router** to reach IP networks.
- The secondary node is an ESP32 classic, which has BLE but **no 802.15.4 radio**.

Secondary factor — shared radio (*to validate*, OPEN-011 in [`../STATUS.md`](../STATUS.md)):

- The FRDM-RW612 offers Wi-Fi 6 plus "Bluetooth LE 5.4 / 802.15.4 radios"
  (NXP UG10160, §2, p.3). Its block diagram includes a *multi-radio coexistence*
  block (UG10160 Fig. 1, p.3), and the board schematic routes BLE and 802.15.4
  through a single RF net labelled `BLE_802.15.4_TR` (FRDM-RW612 schematic, p.1).
- Team understanding: on the RW612, BLE and 802.15.4 share the radio and cannot
  run at the same time. *To validate* against the RW61x datasheet / SDK coexistence
  documentation (not yet in the team's indexed documentation).

Nature of the traffic:

- V2V events (hard brake, obstacle, over-temperature) are small, infrequent
  messages between two vehicles in the current setup.

## Decision

V2V messaging uses **BLE** with a custom GATT service
([`interfaces/ble-gatt.md`](../../interfaces/ble-gatt.md)). Thread/802.15.4 mesh is
kept in the roadmap (M4).

## Consequences

- + Works with the hardware we have: RW612 ↔ ESP32 both speak BLE.
- + Leaves the RW612 free to run Wi-Fi for MQTT in parallel (ADR-0003).
- + Mature tooling for debugging (phone BLE scanners).
- − Point-to-point connections do not scale to many vehicles; a connectionless
  (advertising-based) variant or a mesh will be needed beyond 2 nodes.
- − Range is limited; acceptable for a lab/track demo, to be measured.
- Follow-up: measure latency (REQ-NF-001); revisit with an ADR before M4.

## Alternatives considered

| Alternative | Why not (now) |
|---|---|
| Thread / 802.15.4 mesh | Needs ≥ 3 802.15.4 nodes and a border router; ESP32 classic has no 802.15.4; radio shared with BLE on the RW612 (to validate). Roadmap M4. |
| Wi-Fi (UDP/MQTT) for V2V | Depends on infrastructure (AP/broker); the safety path must work without it (REQ-NF-005). |
| ESP-NOW | ESP32-only; not available on the RW612. |
