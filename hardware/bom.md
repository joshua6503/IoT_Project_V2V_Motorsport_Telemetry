# Bill of materials

| # | Item | Model / part number | Qty | Interface | Supply | Milestone | Status | Datasheet / link |
|---|---|---|---|---|---|---|---|---|
| 1 | Main node | NXP FRDM-RW612 | 1 | — | USB | M1 | available | TBD |
| 2 | Secondary node | ESP32 dev kit (exact model TBD) | 1 | — | USB | M1 | available | TBD |
| 3 | IMU | TBD | 1 | TBD (I2C/SPI) | TBD | M3 | to buy/confirm | TBD |
| 4 | Ultrasonic distance sensor | TBD | 1 | TBD | TBD | M3 | to buy/confirm | TBD |
| 5 | Temperature sensor | TBD | 1 | TBD | TBD | M3 | to buy/confirm | TBD |
| 6 | LED / shift light | TBD | TBD | GPIO | TBD | M1/M3 | TBD | TBD |
| 7 | Buzzer | TBD (active/passive?) | 2 | GPIO/PWM | TBD | M1 | TBD | TBD |
| 8 | Haptic vibration motor + driver | TBD | 1 | GPIO/PWM | TBD | M3 | TBD | TBD |
| 9 | Relay module | TBD | 1 | GPIO | TBD | M3 | TBD | TBD |
| 10 | Level shifter (if 5 V sensors) | TBD | TBD | — | — | M3 | TBD | TBD |
| 11 | Gateway laptop | team PC with Docker | 1 | Wi-Fi | — | M2 | available | — |
| 12 | Wi-Fi access point | TBD (phone hotspot / router) | 1 | — | — | M2 | TBD | — |

## MP1 (DEC-002)

| # | Item | Model / part number | Qty | Interface | Supply | Milestone | Status | Datasheet / link |
|---|---|---|---|---|---|---|---|---|
| 13 | Ultrasonic distance sensor | HC-SR04 (Steren ARD-350) | 1 | GPIO: TRIG D7, ECHO D8 via divider (#21) | 5 V, 15 mA | MP1 | available | [HCSR04.pdf, SparkFun](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf) |
| 14 | Temperature/humidity sensor | DHT11 (Steren ARD-360) | 1 | GPIO D9, single-wire bidirectional | 3.3 V (cable < 100 cm) | MP1 | available | DHT11 datasheet, Aosong |
| 15 | Potentiometer #1 — brake-pressure transducer simulator | value TBD (10 kΩ ideal) | 1 | ADC, Pmod J7-3, via divider (#22) | 3.3 V | MP1 | available | — |
| 16 | Potentiometer #2 — TPS simulator (shift light, optional) | value TBD (10 kΩ ideal) | 1 | ADC, A0 (channel TBD, OPEN-014) | 3.3 V | MP1 | available | — |
| 17 | LED green / yellow / red | TBD | 1 each | GPIO D3 / D4 / D5, active high | 3.3 V | MP1 | available | TBD |
| 18 | Buzzer | type TBD (OPEN-013): 3 unmarked with "+" on top; 1 TMB12A05 (active, 5 V, 25 mA, 2400 ± 200 Hz per the thingbits TMB12A05 sheet) | 1 | GPIO D6 via NPN (#20) | 5 V | MP1 | bench test pending | [LCSC C781864](https://www.lcsc.com/datasheet/C781864.pdf) |
| 19 | Resistor 330 Ω | — | 3 | LED series | — | MP1 | check stock | — |
| 20 | NPN transistor for the buzzer | 2N2222 (or BC547) | 1 | GPIO D6 → base | — | MP1 | to buy | [2N2222A](https://docs.rs-online.com/1ce3/A700000013465269.pdf) |
| 21 | Resistors 2.2 kΩ + 3.3 kΩ | — | 1 + 1 | ECHO divider (≈ 3.0 V, OPEN-015) | — | MP1 | check stock | — |
| 22 | Resistors 27 kΩ + 2.7 kΩ | — | 2 + 2 | pot dividers (0.2–1.2 V ADC window) | — | MP1 | check stock | — |
| 23 | Resistor 1 kΩ | — | 1 | NPN base | — | MP1 | check stock | — |
| 24 | Resistor 5.1 kΩ (optional) | — | 1 | DHT11 pull-up, only for a bare sensor | — | MP1 | check stock | DHT11 datasheet p.3 |
| 25 | Flyback diode | 1N4148 or 1N4007 | 1 | across the buzzer, cathode to +5 V | — | MP1 | to buy | TBD |
| 26 | Breadboard + male-male jumpers | — | 1 | — | — | MP1 | check stock | — |
