# interfaces/ — protocol contracts (source of truth)

Everything that crosses a boundary between components is defined here **before**
it is implemented. See [ADR-0004](../docs/adr/0004-monorepo-contracts-first.md).

| File | Defines | Consumed by |
|---|---|---|
| [`mqtt-topics.md`](mqtt-topics.md) | MQTT topic tree, QoS, retain, example payloads | both nodes, simulator, dashboard |
| [`payload.schema.json`](payload.schema.json) | JSON Schema (draft 2020-12) for MQTT payloads | simulator, integration tests, dashboard |
| [`ble-gatt.md`](ble-gatt.md) | V2V GATT service, characteristics, binary layout | both nodes |
| [`c/v2v_protocol.h`](c/v2v_protocol.h) | C types for the binary V2V packet | RW612 firmware, ESP32 firmware (via copy), unit tests |

## Change procedure

1. Edit the contract here (bump `V2V_PROTOCOL_VERSION_*` if the wire format changes).
2. If the decision is non-trivial, add an ADR in `docs/adr/`.
3. Run `python tools/sync_protocol.py` to refresh the ESP32 copy of the header.
4. Update code and tests. CI fails if the ESP32 copy is stale.

## Identifier mapping

MQTT uses a human-readable `node_id` string; the BLE packet uses a numeric `uint16`.
The mapping is static configuration:

| MQTT `node_id` | BLE `node_id` | Device |
|---|---|---|
| `rw612-a` | `0x0001` | FRDM-RW612 (main node) |
| `esp32-b` | `0x0002` | ESP32 (secondary node) |
| `sim-XX` | `0x0100 + XX` | `tools/node-simulator` (MQTT only) |

## Enum mapping (JSON ↔ C)

| JSON `alert_type` | C `v2v_alert_type_t` | Value |
|---|---|---|
| `manual` | `V2V_ALERT_MANUAL` | `0x01` |
| `hard_brake` | `V2V_ALERT_HARD_BRAKE` | `0x02` |
| `obstacle` | `V2V_ALERT_OBSTACLE` | `0x03` |
| `overtemp` | `V2V_ALERT_OVERTEMP` | `0x04` |

| JSON `severity` | C `v2v_severity_t` | Value |
|---|---|---|
| `info` | `V2V_SEVERITY_INFO` | `0x00` |
| `warning` | `V2V_SEVERITY_WARNING` | `0x01` |
| `critical` | `V2V_SEVERITY_CRITICAL` | `0x02` |

## Open points

- Time base of `ts_ms` / `timestamp_ms`: node uptime for now. SNTP-synced epoch is TBD.
- `cmd` payloads are not yet specified.
