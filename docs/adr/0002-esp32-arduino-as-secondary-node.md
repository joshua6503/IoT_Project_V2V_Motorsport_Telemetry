# ADR-0002: ESP32 classic with Arduino as the secondary node

- **Status:** Accepted
- **Date:** 2026-09-30
- **Deciders:** J. A. Chávez Montero, M. A. Mireles González, K. Rincones Partida, J. Santana Renteria

## Context

- V2V needs at least two nodes; only one FRDM-RW612 is available.
- The team has an ESP32 classic, which has Wi-Fi and BLE — the same two
  transports chosen for the project (ADR-0001, ADR-0003).
- The ESP32 is a test/support node, not the showcase. Engineering effort should
  go to the RW612 firmware.

## Decision

Use an **ESP32 classic** programmed with **Arduino IDE** (Espressif Arduino core)
as node B. It consumes the same contracts in `interfaces/`, through a generated
copy of the C header (`tools/sync_protocol.py`).

## Consequences

- + Fast to bring up: mature BLE and MQTT libraries in the Arduino ecosystem.
- + Exercises the contracts from a second, independent implementation.
- − Arduino hides the RTOS and drivers; this node is not representative of
  production automotive firmware (that is the RW612's role).
- − Arduino only compiles files in the sketch folder → the header must be
  copied, and CI must check the copy is not stale.
- Pure logic from `firmware/common/` can be reused later by copying it the same way.

## Alternatives considered

| Alternative | Why not (now) |
|---|---|
| ESP-IDF | More control, but more setup time for a support node. Possible later. |
| PlatformIO | Would allow relative includes, but adds another toolchain to learn/install for the team. |
| Second RW612 | Not available. |
| Phone app as node B | Useful for debugging, but cannot drive actuators on a vehicle. |
