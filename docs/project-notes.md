# Project notes (decision log)

Short, dated entries. Anything that becomes a lasting decision gets an ADR.

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
