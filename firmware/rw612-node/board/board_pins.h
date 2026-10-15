/**
 * @file board_pins.h
 * @brief Every pin and peripheral instance used by the MP1 node. The ONLY place
 *        where they are written.
 *
 * MP1 pin map DEC-005 (HANDOFF-002/003); sources per row in hardware/pinout.md.
 * Included only by board/board_io.c, after app_config.h (fsl_* types are referenced here).
 *
 * Value formats:
 *   *_PORT, *_PIN  GPIO port and pin for fsl_gpio.h. GPIO_n -> port 0 pin n (n < 32),
 *                  port 1 pin n-32 (n >= 32) [SDK: driver_examples/ctimer/capture/app.h
 *                  "GPIO52: Port 1 Pin 20"; component_examples/memfault/pin_mux.c GPIO_55 = PIO1_23].
 *   *_MUX          IO_MUX_* function from fsl_io_mux.h passed to IO_MUX_SetPinMux().
 *
 * Headers: Arduino D3..D9 [UM12160 p.19-20, Tables 14-15], Pmod J7-3 [SDK:
 * driver_examples/adc/software_trigger/example_board_readme.md].
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/* ------------------------------------------------------------------------- */
/* Status LEDs — external LED + 330 ohm to GND, active high                  */
/* ------------------------------------------------------------------------- */
#define BOARD_LED_GREEN_PORT  0U /* GPIO_15, D3 (J1-8) */
#define BOARD_LED_GREEN_PIN   15U
#define BOARD_LED_GREEN_MUX   IO_MUX_GPIO15
#define BOARD_LED_YELLOW_PORT 0U /* GPIO_18, D4 (J1-10) */
#define BOARD_LED_YELLOW_PIN  18U
#define BOARD_LED_YELLOW_MUX  IO_MUX_GPIO18
#define BOARD_LED_RED_PORT    0U /* GPIO_27, D5 (J1-12) */
#define BOARD_LED_RED_PIN     27U
#define BOARD_LED_RED_MUX     IO_MUX_GPIO27

#define BOARD_LED_ACTIVE_HIGH 1U

/* ------------------------------------------------------------------------- */
/* Shift-light bar — not fitted in MP1 (APP_ENABLE_SHIFT_LIGHT 0)            */
/* ------------------------------------------------------------------------- */
#define BOARD_SHIFT_LED_COUNT      0U
#define BOARD_SHIFT_LED_PORTS      {0U}
#define BOARD_SHIFT_LED_PINS       {0U}
#define BOARD_SHIFT_LED_INIT_MUX() do { } while (0)

/* ------------------------------------------------------------------------- */
/* Buzzer — GPIO_0, D6 (J1-14), through an NPN (high = on).                  */
/* GPIO_0 also lights the on-board blue LED via SJ2 [UM12160 p.19, Table 14]. */
/* Passive: CTIMER0 match 0 on GPIO_0 [SDK: ctimer/simple_pwm/pin_mux.c:63].  */
/* ------------------------------------------------------------------------- */
#define BOARD_BUZZER_PORT             0U
#define BOARD_BUZZER_PIN              0U
#define BOARD_BUZZER_GPIO_MUX         IO_MUX_GPIO0
#define BOARD_BUZZER_PWM_MUX          IO_MUX_CT0_MAT0_OUT
#define BOARD_BUZZER_PWM_CTIMER       CTIMER0
#define BOARD_BUZZER_PWM_CTIMER_ID    0U
#define BOARD_BUZZER_PWM_CLK_ATTACH   kSFRO_to_CTIMER0
#define BOARD_BUZZER_PWM_MATCH        kCTIMER_Match_0
#define BOARD_BUZZER_PWM_PERIOD_MATCH kCTIMER_Match_3 /* as ctimer/simple_pwm/simple_pwm.c:21 */

/* ------------------------------------------------------------------------- */
/* HC-SR04 — TRIG GPIO_20 D7 (J1-16); ECHO GPIO_50 D8 (J2-2) via 2.2k/3.3k   */
/* divider (5 V tolerance unknown, OPEN-015).                                */
/* ------------------------------------------------------------------------- */
#define BOARD_HCSR04_TRIG_PORT 0U
#define BOARD_HCSR04_TRIG_PIN  20U
#define BOARD_HCSR04_TRIG_MUX  IO_MUX_GPIO20
#define BOARD_HCSR04_ECHO_PORT 1U
#define BOARD_HCSR04_ECHO_PIN  18U
#define BOARD_HCSR04_ECHO_MUX  IO_MUX_GPIO50

/* ------------------------------------------------------------------------- */
/* DHT11 — GPIO_52, D9 (J2-4); drive low / release as input                 */
/* ------------------------------------------------------------------------- */
#define BOARD_DHT11_PORT 1U
#define BOARD_DHT11_PIN  20U
#define BOARD_DHT11_MUX  IO_MUX_GPIO52

/* ------------------------------------------------------------------------- */
/* Potentiometers on the ADC (Vref 1.2 V, valid input > 0.2 V)               */
/* ------------------------------------------------------------------------- */
#define BOARD_ADC_BASE     GAU_GPADC0 /* [SDK: adc/software_trigger/app.h] */
#define BOARD_ADC_CH_BRAKE kADC_CH4   /* GPIO_46, Pmod J7-3 [SDK: adc/software_trigger/app.h, example_board_readme.md] */
#if APP_ENABLE_SHIFT_LIGHT
/* TPS on Arduino A0 (GPIO_42): ADC channel unknown — run the boot scan (OPEN-014). */
#ifndef BOARD_ADC_CH_TPS
#error "TBD: BOARD_ADC_CH_TPS — ADC channel of GPIO_42 / A0 (OPEN-014)"
#endif
#endif

/* ------------------------------------------------------------------------- */
/* Free-running microsecond timebase — CTIMER1 from SFRO 16 MHz (DEC-007).   */
/* CTIMER0 is reserved for the passive-buzzer PWM on GPIO_0.                 */
/* ------------------------------------------------------------------------- */
#define BOARD_TIMEBASE_CTIMER     CTIMER1
#define BOARD_TIMEBASE_CTIMER_ID  1U
#define BOARD_TIMEBASE_CLK_ATTACH kSFRO_to_CTIMER1 /* fsl_clock.h, clock_attach_id_t */

#endif /* BOARD_PINS_H */
