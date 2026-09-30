# ADR-0003: Telemetry over Wi-Fi + MQTT with a local Mosquitto broker

- **Status:** Proposed
- **Date:** 2026-09-30
- **Deciders:** J. A. Chávez Montero, M. A. Mireles González, K. Rincones Partida, J. Santana Renteria

## Context

- Nodes must publish telemetry to a dashboard with history (M2).
- On the RW612, Wi-Fi and BLE can run in parallel: the MCUXpresso SDK for
  FRDM-RW612 ships Wi-Fi + BLE coexistence examples (`coex_examples/`, e.g.
  `coex_wifi_peripheral_ht`) and a Wi-Fi MQTT example (`wifi_examples/wifi_mqtt`).
  Concurrent performance on our workload is *to validate* (REQ-NF-004).
- The demo runs in a lab/track without guaranteed Internet.

## Decision

- Transport: **Wi-Fi + MQTT 3.1.1/5** (client choice per node).
- Broker: **Eclipse Mosquitto 2**, running locally in Docker next to **Node-RED**
  (`gateway/docker-compose.yml`).
- Topic tree, QoS and payloads defined in `interfaces/mqtt-topics.md` and
  `interfaces/payload.schema.json`.

## Consequences

- + No cloud dependency or account; works offline on a laptop.
- + MQTT is lightweight and standard for IoT; LWT gives presence detection for free.
- + Node-RED gives a working dashboard quickly.
- − Anonymous, non-TLS broker: **development only**; documented in `mosquitto.conf`.
- − The laptop is a single point of failure for telemetry (not for V2V — BLE path is independent).
- Follow-up: InfluxDB + Grafana for long-term history (M4).

## Alternatives considered

| Alternative | Why not (now) |
|---|---|
| Cloud broker (AWS IoT, HiveMQ Cloud) | Needs Internet and TLS/cert handling from day one. |
| HTTP/REST | Polling, heavier per message, no presence/LWT. |
| CoAP | Less tooling in the team's stack; no advantage for this topology. |
