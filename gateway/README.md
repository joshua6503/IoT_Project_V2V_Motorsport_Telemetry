# gateway — Mosquitto + Node-RED (Docker)

See [ADR-0003](../docs/adr/0003-mqtt-with-local-mosquitto.md).

| Service | Image | Port | Data |
|---|---|---|---|
| `mosquitto` | `eclipse-mosquitto:2` | 1883 (MQTT) | volumes `mosquitto-data`, `mosquitto-log` |
| `node-red` | `nodered/node-red` | 1880 (UI/editor) | volume `node-red-data` |

```powershell
docker compose -f gateway/docker-compose.yml up -d
docker compose -f gateway/docker-compose.yml logs -f mosquitto
docker compose -f gateway/docker-compose.yml down        # keeps volumes
```

Inside the compose network Node-RED reaches the broker as `mosquitto:1883`.
Physical nodes use the **LAN IP of the PC** running Docker (put it in the node config).

> `mosquitto.conf` allows anonymous clients — **development only**.

The Windows firewall may block inbound 1883 from the boards; allow it on the
private network profile if the nodes cannot connect.

Roadmap (M4): add InfluxDB + Grafana services here.
