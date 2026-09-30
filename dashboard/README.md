# dashboard — Node-RED

`node-red/flows.json` is the versioned copy of the Node-RED flows (empty for now).

## Workflow

Node-RED keeps its live state in the `node-red-data` Docker volume. The repo
copy is updated by hand:

1. Open <http://localhost:1880>.
2. **Import**: ☰ → Import → select `dashboard/node-red/flows.json`.
3. After editing: ☰ → Export → *All flows* → Download, and overwrite
   `dashboard/node-red/flows.json`. Commit with `feat(dashboard): ...`.

## Planned (M2)

- MQTT-in on `v2v/+/telemetry`, `v2v/+/alert`, `v2v/+/status` (broker `mosquitto:1883`).
- Live gauges/charts per node, alert log, online/offline indicator.
- Dashboard palette: TBD (`@flowfuse/node-red-dashboard` is the candidate).
- History: Node-RED context/file for M2; InfluxDB + Grafana in M4.
