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
v2v/all/cmd         broker → every node   same commands, one message for all nodes
```

`{node_id}` matches `^[a-z0-9-]{1,32}$` (e.g. `rw612-a`, `esp32-b`, `sim-01`).
**`all` is a reserved `node_id`**: no node may use it. Every node subscribes to its own
`v2v/{node_id}/cmd` and to `v2v/all/cmd`.

> The **safety path is BLE**, not MQTT. `alert` over MQTT is a copy for the
> dashboard and the history; no node may depend on it to trigger an actuator.

## Summary

| Topic | Direction | QoS | Retain | Rate | Schema |
|---|---|---|---|---|---|
| `v2v/{node_id}/telemetry` | node → broker | 0 | no | ≥ 5 Hz (target, to validate) | `#/$defs/telemetry` |
| `v2v/{node_id}/alert` | node → broker | 1 | no | on event | `#/$defs/alert` |
| `v2v/{node_id}/status` | node → broker | 1 | **yes** | on connect / LWT | `#/$defs/status` |
| `v2v/{node_id}/cmd` | broker → node | 1 | no | on demand | `#/$defs/cmd` |
| `v2v/all/cmd` | broker → every node | 1 | no | on demand | `#/$defs/cmd` |

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

### `v2v/rw612-a/cmd` and `v2v/all/cmd`

Frozen by DEC-013 (closes OPEN-010). QoS 1, not retained. Two commands.

Drive an alert on the node's actuator:

```json
{
  "cmd": "alert",
  "seq": 1732371000,
  "origin": "dashboard",
  "alert_type": "manual",
  "severity": "warning",
  "duration_ms": 3000
}
```

Same command relayed by the dashboard for another node's alert (DEC-014):

```json
{
  "cmd": "alert",
  "seq": 1732371001,
  "origin": "dashboard",
  "alert_type": "hard_brake",
  "severity": "critical",
  "duration_ms": 3000,
  "source_node": "rw612-a"
}
```

End the alert early:

```json
{ "cmd": "clear", "seq": 1732371002, "origin": "dashboard" }
```

| Field | Cmd | Required | Type | Notes |
|---|---|---|---|---|
| `cmd` | both | yes | `"alert"` or `"clear"` | |
| `seq` | both | yes | uint32 | sender counter; the sender starts it at a value unlikely to repeat after a restart |
| `origin` | both | yes | string (`node_id` pattern) | who sent it; `"dashboard"` today |
| `alert_type` | alert | yes | `manual`, `hard_brake`, `obstacle`, `overtemp` | same enum as the alert payload |
| `severity` | alert | yes | `info`, `warning`, `critical` | same enum as the alert payload |
| `duration_ms` | alert | no | integer 100–10000 | default 3000 |
| `source_node` | alert | no | string (`node_id` pattern, not `all`) | set when the dashboard relays another node's alert |

Unknown fields are rejected by the schema.

Node behaviour:
- On a valid `alert`, drive the actuator pattern of that severity for `duration_ms` (capped at 10 s).
- A higher-severity local alert wins over the command.
- `clear` ends the commanded alert early.
- An unknown `cmd`, invalid JSON or an out-of-range field is **ignored and logged**.
- Dedupe by `(origin, seq)`: a repeated pair is ignored.
- A received command **never makes the node publish an alert** (no loops).

> This is remote IoT actuation through the broker, not the safety path. Direct V2V stays on BLE
> (F5), and no node may depend on a command to protect the vehicle.
