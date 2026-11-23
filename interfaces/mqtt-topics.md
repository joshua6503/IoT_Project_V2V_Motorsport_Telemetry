# MQTT topics

Broker: Mosquitto on the Raspberry Pi 5 (see `gateway/pi/`), port 1883, **username/password
required** (one user per node: `rw612-a`, `esp32-b`; `dashboard` for the dashboard). Payloads are UTF-8 JSON validated by
[`payload.schema.json`](payload.schema.json).

```
v2v/
└── {node_id}/
    ├── telemetry   node → broker   periodic sensor data
    ├── alert       node → broker   mirror of V2V alerts (dashboard/history)
    ├── status      node → broker   online/offline (retained, LWT)
    └── cmd         broker → node   commands to the node
```

`{node_id}` matches `^[a-z0-9-]{1,32}$` (e.g. `rw612-a`, `esp32-b`, `sim-01`).

> The **safety path is BLE**, not MQTT. `alert` over MQTT is a copy for the
> dashboard and the history; no node may depend on it to trigger an actuator.

## Summary

| Topic | Direction | QoS | Retain | Rate | Schema |
|---|---|---|---|---|---|
| `v2v/{node_id}/telemetry` | node → broker | 0 | no | ≥ 5 Hz (target, to validate) | `#/$defs/telemetry` |
| `v2v/{node_id}/alert` | node → broker | 1 | no | on event | `#/$defs/alert` |
| `v2v/{node_id}/status` | node → broker | 1 | **yes** | on connect / LWT | `#/$defs/status` |
| `v2v/{node_id}/cmd` | broker → node | 1 | no | on demand | TBD |

Rationale:
- **telemetry QoS 0**: high rate; a lost sample is superseded by the next one.
- **alert QoS 1**: must reach the history at least once; consumers dedupe by `(node_id, seq)`.
  Not retained: a stale alert must not be replayed to a new subscriber.
- **status retained + LWT**: a new dashboard subscriber immediately knows who is online.
  The broker publishes `offline` if the node disconnects uncleanly.

## Payload examples

### `v2v/rw612-a/telemetry`

```json
{
  "node_id": "rw612-a",
  "ts_ms": 123456,
  "seq": 812,
  "brake_pct": 42.5,
  "tps_pct": 10.0,
  "temp_c": 36.8,
  "distance_cm": 250,
  "sensor_ok": 7
}
```

| Field | Required | Type | Notes |
|---|---|---|---|
| `node_id` | yes | string | same as in the topic |
| `ts_ms` | yes | uint32 | sender uptime in ms (wraps) |
| `seq` | yes | uint32 | increments per telemetry message |
| `brake_pct`, `tps_pct` | no | number 0–100 | percent |
| `temp_c` | no | number or `null` | `null` on a sensor fault |
| `distance_cm` | no | number ≥ 0 or `null` | `null` = no echo / fault |
| `sensor_ok` | no | uint8 bitmask | bit0 distance, bit1 temperature, bit2 brake (`7` = all valid) |
| `speed_kmh`, `accel_g` | no | number / `{x,y,z}` | reserved for later (IMU/speed) |

Unknown fields are rejected by the schema. A consumer must tolerate missing optional fields.

### `v2v/rw612-a/alert`

```json
{
  "node_id": "rw612-a",
  "ts_ms": 123789,
  "seq": 17,
  "alert_type": "hard_brake",
  "severity": "critical"
}
```

### `v2v/rw612-a/status`

Published retained on connect as `online`; registered as the LWT with `offline`.

```json
{ "node_id": "rw612-a", "state": "online" }
```

### `v2v/rw612-a/cmd`

Command set is **TBD** (not yet in the schema). Proposed shape:

```json
{ "cmd": "actuator_test", "args": { "actuator": "buzzer", "duration_ms": 200 } }
```
