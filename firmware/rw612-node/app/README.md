# app/

Top layer: `main.c`, RTOS task creation, node state machine, wiring of services.
No SDK includes, no register access. Calls `services/` and `firmware/common/`.

| File | What |
|---|---|
| `main_mp1.c`, `app_config.h` | MP1 / PDS: bare-metal superloop, no network (`hello_world` based project). |
| `main_mp2.c`, `app_config_mp2.h` | MP2 / F3: FreeRTOS, tasks `sense` (MP1 loop) and `tlm` (Wi-Fi/MQTT). Replaces `main_mp1.c` in the `wifi_mqtt` based project. |
| `net_config.example.h` | Template for `net_config.h` (Wi-Fi SSID/passphrase, broker, MQTT user/password). |
| `net_config.h` | **Git-ignored**, never committed. |

`app_config.h` (periods, FIR switch, pot calibration) is shared by MP1 and MP2 and is not changed by MP2;
MP2 only adds `app_config_mp2.h`. Details, priorities and the measured fallback: `../README.md` (MP2 / F3).
