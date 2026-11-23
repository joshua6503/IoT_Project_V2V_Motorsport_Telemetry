# Pinout

MP1 rows are DECIDED by DEC-005 (2026-10-14), divider values as wired by DEC-011
(2026-11-10, voltages calculated, not measured); firmware copy in
`firmware/rw612-node/board/board_pins.h`. Other rows stay **TBD**.

Port/pin rule: GPIO_n → port 0 pin n (n < 32), port 1 pin n−32 (n ≥ 32)
[SDK: driver_examples/ctimer/capture/app.h "GPIO52: Port 1 Pin 20";
component_examples/memfault/pin_mux.c GPIO_55 = PIO1_23].

## Node A — FRDM-RW612

| Function | Signal | RW612 pin / GPIO | Header pin | Peripheral | Voltage | Source |
|---|---|---|---|---|---|---|
| Ultrasonic TRIG (HC-SR04) | output | GPIO_20 (port 0, pin 20) | D7, J1-16 | GPIO, `IO_MUX_GPIO20` | 3.3 V out (HC-SR04 at 5 V) | [UM12160 p.18-19, Fig. 13 / Table 14] |
| Ultrasonic ECHO (HC-SR04) | input | GPIO_50 (port 1, pin 18) | D8, J2-2 | GPIO, `IO_MUX_GPIO50` | 5 V → 2 kΩ / 2.7 kΩ divider ≈ 2.9 V (calc., DEC-011; input-high threshold OPEN-015) | [UM12160 p.19, Table 15] |
| Temperature (DHT11 DATA) | bidirectional | GPIO_52 (port 1, pin 20) | D9, J2-4 | GPIO, `IO_MUX_GPIO52` (drive low / release as input) | 3.3 V | [UM12160 p.20, Table 15]; [SDK: ctimer/capture/app.h] |
| Brake pot (pot #1) | analog | GPIO_46 | Pmod J7-3 | `GAU_GPADC0`, `kADC_CH4`, Vref 1.2 V | 0.10–1.00 V calc. (3.3 V → 5.1 kΩ / 2 kΩ pot / 220 Ω, DEC-011) | [SDK: adc/software_trigger/app.h; example_board_readme.md "J7-3(GPIO_46)"] |
| TPS pot (pot #2, optional) | analog | GPIO_42 | A0 (J4) | ADC, channel **TBD (OPEN-014)** | 0.19–1.03 V calc. (3.3 V → 2.7 kΩ / 1 kΩ pot / 220 Ω, DEC-011) | [UM12160 p.18, Fig. 13 "GPIO_42_ARD_A0"] |
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

## Node B — ESP32 (classic, ESP32-WROOM-32 DevKitC-style; exact dev-kit model TBD in `bom.md`)

Assigned in F2 (2026-11-23); mirrored in `firmware/esp32-node/esp32-node/config.example.h`.
Rules applied: ADC1 only (ADC2 pins cannot be used while Wi-Fi is on), no strapping pins as
outputs, no flash pins (GPIO6-11), no input-only pins as outputs.

| Function | Signal | GPIO | Voltage | Source |
|---|---|---|---|---|
| Brake pot wiper | analog | GPIO32 (ADC1_CH4) | 0.15-2.98 V calc. (3.3 V → 5.6 kΩ / 50 kΩ pot / 2.7 kΩ → GND, no capacitor (100 nF optional, not fitted); calculated, not measured) | [ESP-IDF GPIO Summary table "GPIO32 / ADC1_CH4"]; [ESP-IDF GPIO Summary note "ADC2 pins cannot be used when Wi-Fi is used"] |
| LED green | output | GPIO25 | 3.3 V, active high, 220 Ω to GND | ordinary GPIO (not strapping, not flash, not input-only) [ESP32 Datasheet v5.3 §3 Table 3-1] |
| LED yellow | output | GPIO26 | 3.3 V, active high, 220 Ω to GND | same |
| LED red | output | GPIO27 | 3.3 V, active high, 220 Ω to GND | same |
| Buzzer (active) | output | GPIO23 | 3.3 V to NPN base through 1 kΩ (2N2222/BC547); buzzer between VIN (5 V) and collector, emitter GND | same; active buzzer is an ASSUMPTION (OPEN-013) |
| Manual alert | input | GPIO0 (BOOT button) | 3.3 V, on-board pull-up, button to GND, active low | [ESP32-DevKitC V4 User Guide, "Header Block" J3 "GPIO0 … Boot"]; strapping pin, free as normal I/O after reset [ESP32 Datasheet v5.3 §3 "Boot Configurations"] |
| Latency probe (HIL-04) | output | TBD | 3.3 V | TBD |

Avoided: GPIO0/2/5/12/15 are strapping pins (GPIO0 is used only because the BOOT button is
already wired to it; do not hold BOOT during reset or the chip enters download mode)
[ESP32 Datasheet v5.3 §3 Table 3-1: GPIO0, GPIO2, MTDI, MTDO, GPIO5; MTDI = GPIO12, MTDO = GPIO15
(pin names from the same datasheet; the GPIO numbers are standard ESP32 naming, not quoted)].
GPIO6-11 connect to the SPI flash; GPIO34-39 are input-only with no pull-up/down
[ESP-IDF GPIO Summary notes "SPI0/1" and "GPI"]. ADC2 pins (GPIO0, 2, 4, 12-15, 25-27) are
not used for analog input.

Pot: 50 kΩ (Joshua has 50 kΩ and 100 kΩ); 50 kΩ keeps the source impedance lower for the ADC.
12 dB attenuation full-scale (~3.1 V) is an ASSUMPTION: the ESP-IDF ADC page defers the exact
range to the datasheet, which was not read for that table. `BRAKE_MV_MIN/MAX` are bench defaults.

**Power:** USB 5 V (VIN) for the buzzer, 3.3 V for the pot, common GND.

Mirror ESP32 assignments in `firmware/esp32-node/esp32-node/config.example.h`.
