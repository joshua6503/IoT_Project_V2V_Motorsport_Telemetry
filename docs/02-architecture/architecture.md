# Architecture

## Two planes

| Plane | Transport | Purpose | Criticality |
|---|---|---|---|
| **V2V** | BLE, custom GATT (`interfaces/ble-gatt.md`) | vehicle ↔ vehicle critical events | safety-relevant, must work alone |
| **IoT** | Wi-Fi + MQTT (`interfaces/mqtt-topics.md`) | telemetry, presence, history | informational |

```mermaid
flowchart LR
    subgraph A["Vehicle A — FRDM-RW612"]
        SA[Sensors] --> EA[Edge logic<br/>alert_evaluate]
        EA --> VA[v2v_ service]
        EA --> TA[tlm_ service]
        VA --> AA[Actuators]
    end
    subgraph B["Vehicle B — ESP32"]
        SB[Button / sensors] --> EB[Edge logic]
        EB --> VB[V2V BLE]
        EB --> TB[MQTT client]
        VB --> AB[LED / buzzer]
    end
    VA <-- "BLE GATT<br/>v2v_packet_t" --> VB
    TA -- "MQTT v2v/rw612-a/#" --> BR[(Mosquitto)]
    TB -- "MQTT v2v/esp32-b/#" --> BR
    SIM[node-simulator] -. MQTT .-> BR
    BR --> NR[Node-RED dashboard]
```

## Four layers (school deliverable view)

```mermaid
flowchart TB
    L1["1. Perception & Actuation<br/>IMU, ultrasonic, temperature, button<br/>LED/shift light, buzzer, haptic, relay<br/><i>RW612, ESP32</i>"]
    L2["2. Communication<br/>BLE GATT (V2V) · Wi-Fi + MQTT (IoT)"]
    L3["3. Processing<br/>Edge: alert_evaluate on the RW612<br/>Gateway: Mosquitto, Node-RED flows"]
    L4["4. Application<br/>Node-RED dashboard, history, alerts<br/><i>roadmap: InfluxDB + Grafana</i>"]
    L1 --> L2 --> L3 --> L4
    L3 -. "alert received → actuate" .-> L1
```

The RW612 belongs to **both** layer 1 (it reads sensors and drives actuators)
and layer 3 (it runs the edge logic that decides whether an event is critical).

## Alert sequence (M1/M3)

```mermaid
sequenceDiagram
    participant S as Sensor/button (A)
    participant A as Node A
    participant B as Node B
    participant X as Actuator (B)
    participant M as Broker
    S->>A: sample / press
    A->>A: alert_evaluate()
    A->>B: BLE Alert (v2v_packet_t)
    B->>B: check version, dedupe (node_id, seq)
    B->>X: activate (pattern by severity)
    A-->>M: v2v/A/alert (mirror, QoS 1)
```

## RW612 firmware layers

See [ADR-0005](../adr/0005-layered-firmware-architecture.md).

```mermaid
flowchart TB
    APP[app/ — tasks, state machine] --> SRV[services/ — v2v_, tlm_, alert_]
    SRV --> DRV[drivers/ — IMU, distance, temp, buzzer…]
    DRV --> BRD[board/ — pinmux, clocks, SDK wrappers]
    BRD --> SDK[(NXP MCUXpresso SDK<br/>not in repo)]
    APP --> COM[firmware/common — pure logic]
    SRV --> COM
    COM --> IF[interfaces/c/v2v_protocol.h]
```

## Deployment

| Component | Runs on | Build |
|---|---|---|
| rw612-node | FRDM-RW612 | MCUXpresso IDE + SDK (local only) |
| esp32-node | ESP32 dev kit (model TBD) | Arduino IDE / arduino-cli |
| gateway | team laptop, Docker | `docker compose` |
| dashboard | Node-RED container | import `flows.json` |
| node-simulator | any PC, Python 3.10+ | `pip install -r requirements.txt` |
