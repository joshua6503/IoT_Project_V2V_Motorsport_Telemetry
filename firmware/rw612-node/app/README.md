# app/

Top layer: `main.c`, RTOS task creation, node state machine, wiring of services.
No SDK includes, no register access. Calls `services/` and `firmware/common/`.
