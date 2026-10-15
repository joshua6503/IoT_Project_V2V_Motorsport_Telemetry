/**
 * @file alert_pattern.h
 * @brief Pure mapping from an alert to the desired actuator state (DEC-004).
 *
 * Hardware-independent: returns what the LEDs and buzzer should be doing at a
 * given instant. Blinking is derived from now_ms, so no timers are needed; the
 * caller just applies the result every loop iteration.
 */
#ifndef ALERT_PATTERN_H
#define ALERT_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

#include "alert_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DEC-004 timing (ms). */
#define ALERT_PATTERN_WARN_PERIOD_MS  1000u /**< WARNING buzzer beep: 1 Hz */
#define ALERT_PATTERN_WARN_BEEP_MS    100u  /**< WARNING buzzer on-time per beep */
#define ALERT_PATTERN_CRIT_PERIOD_MS  250u  /**< CRITICAL red blink + beep: 4 Hz */
#define ALERT_PATTERN_CRIT_ON_MS      125u  /**< CRITICAL on-time (50 % duty) */
#define ALERT_PATTERN_FAULT_PERIOD_MS 1000u /**< SENSOR FAULT red/yellow alternation: 1 Hz */

/** Desired actuator state for one instant. */
typedef struct
{
    bool led_green;
    bool led_yellow;
    bool led_red;
    bool buzzer_on;
} alert_pattern_t;

/**
 * @brief Actuator state for alert @p a at time @p now_ms (DEC-004).
 *
 * - NONE:         green on, buzzer off.
 * - WARNING:      yellow on, buzzer beeps 1 Hz (100 ms on).
 * - CRITICAL:     red blinks 4 Hz, buzzer on together with red.
 * - SENSOR FAULT: red/yellow alternate at 1 Hz, buzzer off. Shown only when there
 *                 is no active alert: a valid alert is never hidden by a fault.
 *
 * @param a       Alert from alert_evaluate(); NULL is treated as NONE.
 * @param now_ms  Current time in ms (any monotonic base, wrap-safe).
 */
alert_pattern_t alert_pattern_for(const alert_t *a, uint32_t now_ms);

/**
 * @brief Number of shift-light LEDs to light for a throttle position.
 * @param tps_pct  Throttle 0-100 %; values outside are clamped.
 * @param n_leds   LEDs in the bar (0 -> always 0).
 * @return 0..n_leds, floor(tps_pct * n_leds / 100).
 */
uint8_t alert_shift_level(float tps_pct, uint8_t n_leds);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_PATTERN_H */
