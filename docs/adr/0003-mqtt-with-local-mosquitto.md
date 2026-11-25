# ADR-0003: Telemetry over Wi-Fi + MQTT with a Mosquitto broker on a Raspberry Pi 5

- **Status:** Accepted
- **Date:** 2026-11-23 (rewrites the 2026-09-30 proposal, which used a laptop with Docker);
  security section revised 2026-11-25 (credentials + ACL)
- **Deciders:** J. A. Chávez Montero, M. A. Mireles González, K. Rincones Partida, J. Santana Renteria

## Context

- MP2 is graded on both nodes (`rw612-a`, `esp32-b`) publishing directly to a broker over Wi-Fi;
  BLE V2V moves to the next phase (F5).
- The demo runs in a lab/track without guaranteed Internet or a network we control, so the
  gateway must be able to provide its own 2.4 GHz network.
- The Raspberry Pi 5 has a single Wi-Fi radio: it cannot be a client and an access point at once.
- The Pi must stay maintainable (apt, updates, SSH) when it is not serving the nodes.
- On the RW612, Wi-Fi and BLE can run in parallel (SDK `coex_examples/`); concurrent performance
  is to validate (REQ-NF-004).

## Decision

- Transport: **Wi-Fi + MQTT 3.1.1/5**. Topics, QoS and payloads: `interfaces/mqtt-topics.md`,
  `interfaces/payload.schema.json`.
- Gateway: **Raspberry Pi 5 + Eclipse Mosquitto 2.0**, listener 1883 on all interfaces,
  set up by `gateway/pi/setup_pi.sh`. No TLS (closed lab network).
- Security, two layers:
  - **Credentials:** `allow_anonymous false` and a `password_file` with one user per node
    (`rw612-a`, `esp32-b`) plus `dashboard`. The **username of a node is its `node_id`**.
    Passwords are typed by a team member at a hidden prompt; they are never in the repo, in
    scripts or on a command line.
  - **Access control list** (`gateway/pi/acl`, installed to `/etc/mosquitto/acl`, `root:mosquitto`
    0640, enabled with `acl_file`; everything not allowed is denied). `pattern` rules apply to
    every user and use `%u`, so a node can only write `v2v/%u/{telemetry,alert,status,sensor/#}`
    (`status` covers the retained `online` and the LWT, whose write permission Mosquitto checks at
    connect) and read `v2v/%u/cmd` and `v2v/all/cmd`. `dashboard` reads `v2v/#`, writes
    `v2v/+/cmd` and reads/writes `v2v/verify/#` (used by `verify-pi`). A node therefore cannot
    publish as another node, and the dashboard cannot forge telemetry, alerts or status.
- Two network modes on the Pi, switched with `gateway/pi/pi-mode.sh`:
  - **home**: wlan0 is a client of the home Wi-Fi (the phone hotspot stays as a lower-priority
    fallback). For maintenance, updates and SSH.
  - **pit**: wlan0 is the access point **V2V-PIT** (2.4 GHz, fixed channel, WPA2), Pi at the fixed
    address `10.42.0.1`. Both nodes and the laptop join it. Mode for node work and the demo.
  The nodes are always configured for V2V-PIT, so a mode change never means rebuilding firmware.
- Every switch is a trial: detached from SSH, automatic rollback after 5 minutes unless
  `pi-mode confirm` is run through the new network. The Pi boots in the last confirmed mode.
- Dashboard: **Node-RED + `@flowfuse/node-red-dashboard` on the team laptop**, MQTT user `dashboard`.

## Consequences

- + Works with no Internet and no school/phone network in pit mode.
- + MQTT is lightweight and standard; LWT gives presence detection.
- + Password auth keeps stray clients on the AP from publishing.
- + With the ACL, a leaked node password only lets an attacker publish as that one node; it cannot
  spoof other nodes or send commands.
- − `username == node_id` is a hard rule; `dashboard` and `all` are reserved names.
- − Adding a user that is not a node needs a new `user` block in `gateway/pi/acl`.
- − `tools/node-simulator` uses one user, so it can no longer publish as several nodes on the Pi
  (development only; use the laptop broker or one user per simulated node).
- − A denied publish is silent in MQTT 3.1.1; a misconfigured node just looks as if it sends nothing.
- − In pit mode the Pi has no uplink (no apt/updates): do that in home mode first.
- − A laptop joined to V2V-PIT loses its other Internet unless it has a second link (e.g. USB tethering).
- − Unencrypted MQTT; acceptable only on the closed AP.
- − The Pi is a single point of failure for telemetry (not for V2V: the safety path is BLE).
- Follow-up: InfluxDB + Grafana for long-term history (M4).

## Alternatives considered

| Alternative | Why not |
|---|---|
| Broker on the laptop (Docker/native) | Laptop is also the dashboard and changes networks; not a stable demo address. Kept only for local development (`gateway/docker-compose.yml`). |
| Pi as client of a school/phone network | Depends on a network we do not control; IPs change. |
| Pi with a second USB Wi-Fi adapter (client + AP at once) | Extra hardware; not needed if modes are switched deliberately. |
| Cloud broker (AWS IoT, HiveMQ Cloud) | Needs Internet and TLS/cert handling. |
| HTTP/REST, CoAP | Polling / no LWT; less tooling. |
