# Pinout

MP1 rows are DECIDED by DEC-005 (2026-10-14); firmware copy in
`firmware/rw612-node/board/board_pins.h`. Other rows stay **TBD**.

Port/pin rule: GPIO_n → port 0 pin n (n < 32), port 1 pin n−32 (n ≥ 32)
[SDK: driver_examples/ctimer/capture/app.h "GPIO52: Port 1 Pin 20";
component_examples/memfault/pin_mux.c GPIO_55 = PIO1_23].

## Node A — FRDM-RW612

| Function | Signal | RW612 pin / GPIO | Header pin | Peripheral | Voltage | Source |
|---|---|---|---|---|---|---|
| Ultrasonic TRIG (HC-SR04) | output | GPIO_20 (port 0, pin 20) | D7, J1-16 | GPIO, `IO_MUX_GPIO20` | 3.3 V out (HC-SR04 at 5 V) | [UM12160 p.18-19, Fig. 13 / Table 14] |
| Ultrasonic ECHO (HC-SR04) | input | GPIO_50 (port 1, pin 18) | D8, J2-2 | GPIO, `IO_MUX_GPIO50` | 5 V → 2.2 kΩ / 3.3 kΩ divider ≈ 3.0 V (OPEN-015) | [UM12160 p.19, Table 15] |
| Temperature (DHT11 DATA) | bidirectional | GPIO_52 (port 1, pin 20) | D9, J2-4 | GPIO, `IO_MUX_GPIO52` (drive low / release as input) | 3.3 V | [UM12160 p.20, Table 15]; [SDK: ctimer/capture/app.h] |
| Brake pot (pot #1) | analog | GPIO_46 | Pmod J7-3 | `GAU_GPADC0`, `kADC_CH4`, Vref 1.2 V | 0.2–1.2 V (27 kΩ / pot / 2.7 kΩ divider) | [SDK: adc/software_trigger/app.h; example_board_readme.md "J7-3(GPIO_46)"] |
| TPS pot (pot #2, optional) | analog | GPIO_42 | A0 (J4) | ADC, channel **TBD (OPEN-014)** | 0.2–1.2 V | [UM12160 p.18, Fig. 13 "GPIO_42_ARD_A0"] |
| LED green | output | GPIO_15 (port 0, pin 15) | D3, J1-8 | GPIO, `IO_MUX_GPIO15`, active high (330 Ω to GND) | 3.3 V | [UM12160 p.19, Table 14] |
| LED yellow | output | GPIO_18 (port 0, pin 18) | D4, J1-10 | GPIO, `IO_MUX_GPIO18`, active high | 3.3 V | [UM12160 p.19, Table 14] (shared with Pmod J7-2, unused) |
| LED red | output | GPIO_27 (port 0, pin 27) | D5, J1-12 | GPIO, `IO_MUX_GPIO27`, active high | 3.3 V | [UM12160 p.19, Table 14] |
| Buzzer | output | GPIO_0 (port 0, pin 0) | D6, J1-14 | Active: GPIO `IO_MUX_GPIO0`, high = on (NPN). Passive: `IO_MUX_CT0_MAT0_OUT`, CTIMER0 match 0. Also lights the on-board blue LED via SJ2. | 3.3 V to NPN base (buzzer at 5 V) | [UM12160 p.19, Table 14]; [SDK: ctimer/simple_pwm/pin_mux.c] |
| Shift light (optional, not used in MP1) | output | GPIO_12 green / GPIO_1 red (on-board RGB) | — | GPIO | 3.3 V | [UM12160 p.9, Table 5] |
| Debug console | UART | GPIO_24 | MCU-Link VCOM (J10) | FLEXCOMM3, 115200 8N1 | 3.3 V | [SDK: hello_world pin_mux.c `IO_MUX_FC3_USART_DATA`, board.h] |
| Timebase (internal) | — | — | — | CTIMER1, SFRO 16 MHz → 1 MHz (DEC-007) | — | [SDK: ctimer/simple_match/hardware_init.c; fsl_clock.h `kSFRO_to_CTIMER1`] |
| Test button (M1) | input | TBD (on-board user button?) | TBD | GPIO | 3.3 V | TBD |
| Haptic motor | output (via driver) | TBD | TBD | GPIO / PWM | TBD | TBD |
| Relay | output (via driver) | TBD | TBD | GPIO | TBD | TBD |
| IMU SDA / SCL | I2C | TBD | TBD | Flexcomm I2C (TBD) | 3.3 V | TBD |
| Latency probe (HIL-04) | output | TBD | TBD | GPIO | 3.3 V | TBD |

**Do not use in MP1:** GPIO_11 (D2, shared with SW2 via R220 [UM12160 p.8, Table 4]),
GPIO_8/9 (D0/D1, FC1), GPIO_6/7 (D10, D13) [UM12160 p.19-20].

**Power:** 5 V from J3-10 (HC-SR04, buzzer) [UM12160 p.10, Table 6, note 1]; 3.3 V from
3V3_OUT on J3 (DHT11, pots; pin 8 is an ASSUMPTION from the Fig. 13 order) [UM12160 p.18];
common GND.

## Node B — ESP32

| Function | Signal | GPIO | Voltage | Source |
|---|---|---|---|---|
| Test button | input | TBD | 3.3 V | TBD |
| Alert LED | output | TBD | 3.3 V | TBD |
| Buzzer | output | TBD | TBD | TBD |
| Latency probe (HIL-04) | output | TBD | 3.3 V | TBD |

Mirror ESP32 assignments in `firmware/esp32-node/esp32-node/config.example.h`.
