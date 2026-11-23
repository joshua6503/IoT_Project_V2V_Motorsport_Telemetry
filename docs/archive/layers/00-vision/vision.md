# Vision

## Problem

In amateur and student motorsport (Formula Student, karting, Baja SAE) drivers
get little information about what happens **ahead of them**: a car braking hard
behind a crest, an obstacle on the track, a car overheating in the pit lane.
Teams also lack an affordable way to log and view live telemetry.

## Product

**V2V Motorsport Telemetry** turns each vehicle into a connected node that:

1. **Measures and publishes** its telemetry (speed, acceleration, temperature,
   distance) to a local dashboard — the IoT plane.
2. **Shares critical events** directly with nearby vehicles over BLE — the V2V
   plane — and reacts to events received from others with an actuator
   (shift light/LED, buzzer, haptic motor, relay).

Critical events are detected **at the edge**, on the node, so the V2V path
works even if Wi-Fi or the broker are down.

## Users

| User | Needs |
|---|---|
| Driver | Immediate, unambiguous warning (light/sound/vibration) |
| Team engineer | Live telemetry and history per vehicle |
| Race control (future) | Overview of all vehicles and alerts |

## Scope

In scope: 2 nodes (FRDM-RW612 + ESP32), local broker and dashboard, 3 alert
types + a manual test alert, lab/track demo.

Out of scope (for now): certified safety function, security hardening, more than
2 vehicles, cloud, mesh networking (M4 roadmap).

## Success criteria

- M1: button on node A → actuator on node B, reliably.
- M2: both nodes visible live on the dashboard with history.
- M3: real sensors trigger real alerts end to end.
- Measured latency and rate reported against the targets in `requirements.md`.
