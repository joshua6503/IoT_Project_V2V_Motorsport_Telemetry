# ADR-0004: Monorepo with a contracts-first approach

- **Status:** Accepted
- **Date:** 2026-09-30
- **Deciders:** J. A. Chávez Montero, M. A. Mireles González, K. Rincones Partida, J. Santana Renteria

## Context

- Four components (RW612 firmware, ESP32 firmware, gateway, dashboard) built by
  four people must agree on topics, payloads and a binary BLE packet.
- Protocol drift between two firmwares is the most likely integration failure.

## Decision

- One **monorepo** for firmware, gateway, dashboard, tools, tests and docs.
- `interfaces/` is the **single source of truth** for every protocol:
  MQTT topics, JSON Schema, BLE GATT layout and the C header `v2v_protocol.h`.
- Rule: a protocol change is made in `interfaces/` first, then in code.
- Consumers that cannot include from `interfaces/` directly (Arduino) receive a
  generated copy; CI fails if the copy is stale.

## Consequences

- + One PR can change a contract and all its consumers atomically.
- + Contracts are testable: JSON payloads validated against the schema, C layout
  checked by unit tests (`offsetof`/`sizeof`).
- + Good portfolio story: the repo shows the whole system.
- − CI must handle heterogeneous toolchains (gcc, arduino-cli, Python).
- − The NXP SDK cannot be committed, so the RW612 is not built in CI.

## Alternatives considered

| Alternative | Why not (now) |
|---|---|
| One repo per component | Contracts would be duplicated and drift. |
| Protobuf / nanopb for V2V | Extra codegen toolchain on both firmwares; a 16-byte fixed struct is enough for now. |
| Git submodule for `interfaces/` | Adds friction for a 4-person school team. |
