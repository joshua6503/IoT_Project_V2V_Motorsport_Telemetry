/**
 * @file app_config.h
 * @brief MP1 build-time configuration (periods, buzzer kind, shift light, pot calibration).
 *
 * Project-wide settings header, like the SDK's mcux_config.h: any layer may read
 * it. Pins do NOT go here (board/board_pins.h). Alert thresholds do NOT go here
 * (alert_config_default() in firmware/common).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------- */
/* Loop periods (ms)                                                         */
/* ------------------------------------------------------------------------- */
#define APP_SENSOR_PERIOD_MS 50u   /**< pots + evaluate */
#define APP_HCSR04_PERIOD_MS 100u  /**< HC-SR04 (DEC-006; datasheet cycle > 60 ms) */
#define APP_DHT11_PERIOD_MS  2500u /**< DHT11 read (DEC-006; datasheet sampling > 2 s) */
#define APP_LOG_PERIOD_MS    200u  /**< one UART log line */

/* ------------------------------------------------------------------------- */
/* Boot ADC scan (OPEN-014): print CH0..CH7 raw, 3 passes, 500 ms apart      */
/* ------------------------------------------------------------------------- */
#define APP_ADC_SCAN_AT_BOOT    1
#define APP_ADC_SCAN_PASSES     3u
#define APP_ADC_SCAN_PERIOD_MS  500u

/* ------------------------------------------------------------------------- */
/* Buzzer kind (OPEN-013). Switching kind is a one-line change below.        */
/* ------------------------------------------------------------------------- */
#define BUZZER_KIND_ACTIVE  1 /**< has its own oscillator: GPIO on/off */
#define BUZZER_KIND_PASSIVE 2 /**< needs a square wave: CTIMER PWM */

#define BUZZER_KIND BUZZER_KIND_ACTIVE /* ASSUMPTION — OPEN-013 (bench test pending) */

#define BUZZER_PASSIVE_HZ 2700u /* ASSUMPTION — OPEN-013: tone for a passive buzzer, TODO(datasheet) resonant frequency */

/* ------------------------------------------------------------------------- */
/* Shift light (potentiometer #2 = TPS simulator, DEC-002, optional)         */
/* ------------------------------------------------------------------------- */
#define APP_ENABLE_SHIFT_LIGHT 0  /**< 1 to drive the shift-light bar */
#define APP_SHIFT_LEDS         0u /**< LEDs in the bar; clamped to the board count */

/* ------------------------------------------------------------------------- */
/* Potentiometer calibration (raw ADC counts at 0 % and 100 %)               */
/* ------------------------------------------------------------------------- */
#define APP_POT_BRAKE_RAW_MIN 0     /* ASSUMPTION (bench default, calibrate on the bench; see TODO(tutor) ADC format in board_io.c) */
#define APP_POT_BRAKE_RAW_MAX 32767 /* ASSUMPTION (bench default, calibrate on the bench; see TODO(tutor) ADC format in board_io.c) */
#define APP_POT_TPS_RAW_MIN   0     /* ASSUMPTION (bench default, calibrate on the bench; see TODO(tutor) ADC format in board_io.c) */
#define APP_POT_TPS_RAW_MAX   32767 /* ASSUMPTION (bench default, calibrate on the bench; see TODO(tutor) ADC format in board_io.c) */

#endif /* APP_CONFIG_H */
