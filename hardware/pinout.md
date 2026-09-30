# Pinout

All entries **TBD**. Fill the *Source* column with the document and page used
(e.g. FRDM-RW612 board user manual / schematic page) when a pin is assigned.

## Node A — FRDM-RW612

| Function | Signal | RW612 pin / GPIO | Header pin | Peripheral | Voltage | Source |
|---|---|---|---|---|---|---|
| Test button (M1) | input | TBD (on-board user button?) | TBD | GPIO | 3.3 V | TBD |
| Alert LED (M1) | output | TBD (on-board RGB LED?) | TBD | GPIO | 3.3 V | TBD |
| Buzzer | output | TBD | TBD | GPIO / PWM (CTIMER?) | TBD | TBD |
| Haptic motor | output (via driver) | TBD | TBD | GPIO / PWM | TBD | TBD |
| Relay | output (via driver) | TBD | TBD | GPIO | TBD | TBD |
| IMU SDA / SCL | I2C | TBD | TBD | Flexcomm I2C (TBD) | 3.3 V | TBD |
| Ultrasonic TRIG | output | TBD | TBD | GPIO | TBD (level shift?) | TBD |
| Ultrasonic ECHO | input | TBD | TBD | GPIO / CTIMER capture | TBD (level shift?) | TBD |
| Temperature | TBD (I2C / 1-Wire / ADC) | TBD | TBD | TBD | TBD | TBD |
| Latency probe (HIL-04) | output | TBD | TBD | GPIO | 3.3 V | TBD |

## Node B — ESP32

| Function | Signal | GPIO | Voltage | Source |
|---|---|---|---|---|
| Test button | input | TBD | 3.3 V | TBD |
| Alert LED | output | TBD | 3.3 V | TBD |
| Buzzer | output | TBD | TBD | TBD |
| Latency probe (HIL-04) | output | TBD | 3.3 V | TBD |

Mirror ESP32 assignments in `firmware/esp32-node/esp32-node/config.example.h`.
