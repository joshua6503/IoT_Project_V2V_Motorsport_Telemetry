# Project notes (decision log)

Short, dated entries. Anything that becomes a lasting decision gets an ADR.

## 2026-09-30

- Repository scaffold created (structure, contracts, CI, docs). No firmware logic yet.
- ADR-0001..0005 accepted: BLE for V2V, ESP32/Arduino as node B, local
  Mosquitto, contracts-first monorepo, layered firmware.
- Protocol v0.1: 16-byte `v2v_packet_t`, GATT service UUID base `2b7b0001-fbc4-4f74-84ba-26504f32ca72`.
- Locally installed MCUXpresso SDK for FRDM-RW612 reports version 26.06.00 in its
  manifest; team-wide SDK version still TBD.
- To validate: BLE and 802.15.4 cannot run simultaneously on the RW612
  (assumption behind ADR-0001; not yet confirmed in the documentation we have).
- Pending: pin assignment, exact sensor/actuator models, ESP32 dev-kit model,
  alert thresholds, time base.
