# drivers/

Device drivers for external parts (IMU, distance sensor, temperature sensor,
buzzer, haptic, relay). Each driver exposes a small hardware-agnostic API and
talks to the MCU only through `board/`. Part models are TBD (`hardware/bom.md`).
