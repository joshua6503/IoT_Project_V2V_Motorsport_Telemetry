# BLE GATT — V2V service

Binary layouts in this document are defined in C by
[`c/v2v_protocol.h`](c/v2v_protocol.h). If they ever disagree, **the header wins**
and this document gets fixed.

## UUIDs

128-bit base generated with Python `uuid.uuid4()` on 2026-09-30. The low 16 bits
of the first group identify the attribute.

| Attribute | UUID | Properties |
|---|---|---|
| V2V Service | `2b7b0001-fbc4-4f74-84ba-26504f32ca72` | primary |
| Alert characteristic | `2b7b0002-fbc4-4f74-84ba-26504f32ca72` | notify, write without response |
| NodeInfo characteristic | `2b7b0003-fbc4-4f74-84ba-26504f32ca72` | read |

## Alert characteristic — `v2v_packet_t` (16 bytes, little-endian)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | `version` | `V2V_PROTOCOL_VERSION` (`0x01` = v0.1). Receivers drop packets with an unknown major. |
| 1 | 1 | `alert_type` | `v2v_alert_type_t` |
| 2 | 1 | `severity` | `v2v_severity_t` |
| 3 | 1 | `flags` | bit0 `V2V_FLAG_CRC_PRESENT` |
| 4 | 2 | `node_id` | sender id (mapping in [`README.md`](README.md)) |
| 6 | 2 | `seq` | per-sender counter; receiver dedupes by `(node_id, seq)` |
| 8 | 4 | `timestamp_ms` | sender uptime (time base TBD) |
| 12 | 2 | `crc16` | CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`) over bytes 0..11; valid only if the flag is set |
| 14 | 2 | `reserved` | `0` |

Example — `hard_brake`, `critical`, node `0x0001`, seq 17, ts 123789 ms, no CRC:

```
01 02 02 00 | 01 00 | 11 00 | 8D E3 01 00 | 00 00 | 00 00
ver typ sev flg  node    seq    timestamp     crc16   rsvd
```

Why an optional application CRC if BLE already has one: the link-layer CRC only
protects the air interface; this one protects the application path (buffer copies,
bridges, a future relay over another transport). It is optional to keep M1 simple.

## NodeInfo characteristic — `v2v_node_info_t` (8 bytes, read)

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | `version` |
| 1 | 1 | `capabilities` (`V2V_CAP_LED \| BUZZER \| HAPTIC \| RELAY`) |
| 2 | 2 | `node_id` |
| 4 | 1 | `fw_major` |
| 5 | 1 | `fw_minor` |
| 6 | 1 | `fw_patch` |
| 7 | 1 | `reserved` |

## Roles (proposal — to validate in M1)

| Node | GAP role | GATT role | Sends alerts via | Receives alerts via |
|---|---|---|---|---|
| FRDM-RW612 | Peripheral (advertises the V2V service UUID) | Server | Notify on Alert | Central writes Alert |
| ESP32 | Central (scans for the V2V service UUID) | Client | Write Without Response on Alert | Subscribes to Alert notifications |

Reasoning:
- One connection covers both directions: notify (server → client) and write (client → server).
- The RW612 as peripheral/server keeps its firmware close to the SDK peripheral
  examples listed in [`firmware/rw612-node/README.md`](../firmware/rw612-node/README.md).
- Roles can be swapped without changing the packet format.

To validate:
- [ ] End-to-end latency with the chosen connection interval (REQ-NF-001).
- [ ] Behaviour on disconnection: reconnect time, alerts lost during the gap.
- [ ] More than 2 vehicles: evaluate a connectionless alternative (alert inside
      extended advertising / manufacturer data) — needs its own ADR.
- [ ] Security (pairing/bonding) is out of scope for M1; decide before any public demo.
