# ADR-0005: Layered firmware architecture (App → Services → Drivers → Board)

- **Status:** Proposed
- **Date:** 2026-09-30
- **Deciders:** J. A. Chávez Montero, M. A. Mireles González, K. Rincones Partida, J. Santana Renteria

## Context

- The RW612 firmware mixes vendor SDK code (drivers, BLE stack, Wi-Fi, RTOS)
  with our own logic.
- We want to unit-test decision logic on a PC and keep the option of porting it
  (to the ESP32 or another MCU) — a common expectation in automotive embedded work.

## Decision

Four layers, dependencies only point downward:

| Layer | Folder | Contains | May use the NXP SDK? |
|---|---|---|---|
| App | `firmware/rw612-node/app/` | tasks, state machine, wiring of services | no |
| Services | `firmware/rw612-node/services/` | `v2v_` (BLE), `tlm_` (MQTT), `alert_` actuation | no — only driver interfaces |
| Drivers | `firmware/rw612-node/drivers/` | sensor/actuator drivers (IMU, ultrasonic, temp, buzzer…) | no — only `board/` |
| Board | `firmware/rw612-node/board/` | pin mux, clocks, SDK peripheral wrappers | **yes, only here** |

Hardware-independent logic (alert rules, packet encode/decode, filters) lives in
`firmware/common/` and has unit tests in `tests/unit/`.

## Consequences

- + Pure logic runs on the host with gcc + Unity, and in CI.
- + Swapping a sensor model touches only `drivers/`; changing the board touches only `board/`.
- − More files and interfaces than a single-`main.c` example.
- − Discipline required: no `#include "fsl_*.h"` outside `board/` (to be checked in review).

## Alternatives considered

| Alternative | Why not (now) |
|---|---|
| Extend an SDK example as-is | Fast, but mixes SDK and logic; nothing testable on a PC. |
| Zephyr RTOS | Strong HAL story, but the team's course and tooling are MCUXpresso IDE + SDK. |
| AUTOSAR-like layering | Too heavy for the scope; this ADR borrows the idea of separating application from BSW. |
