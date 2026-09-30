# firmware

| Folder | Target | Toolchain | Built in CI |
|---|---|---|---|
| [`common/`](common/) | any (pure C99) | host gcc for tests; compiled into both nodes | yes (unit tests) |
| [`rw612-node/`](rw612-node/) | NXP FRDM-RW612 | MCUXpresso IDE + SDK | no (SDK not in repo) |
| [`esp32-node/`](esp32-node/) | ESP32 classic | Arduino IDE / arduino-cli | yes (compile only) |

## common/

Hardware-independent logic: no SDK, no RTOS, no I/O, no dynamic memory.
Every function has a unit test in `tests/unit/`.

| Module | Purpose | State |
|---|---|---|
| `alert_logic` | `alert_evaluate()`: telemetry sample → alert decision | stub |
| `v2v_codec` (planned) | encode/decode `v2v_packet_t`, CRC-16 | not started |

## Include paths

Both nodes need:

```
interfaces/c
firmware/common/include
```

and must compile `firmware/common/src/*.c`. The ESP32 gets these through
`tools/sync_protocol.py` (see [`esp32-node/README.md`](esp32-node/README.md)).

## Naming

- Files and functions in `snake_case`.
- Module prefix on every public symbol: `v2v_` (BLE V2V), `tlm_` (telemetry/MQTT), `alert_` (alert logic and actuation).
