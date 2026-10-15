/**
 * @file alert_actuation.h
 * @brief Applies the DEC-004 pattern (alert_pattern_for) to the LEDs and buzzer.
 */
#ifndef ALERT_ACTUATION_H
#define ALERT_ACTUATION_H

#include <stdint.h>

#include "alert_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise LEDs and buzzer (all off). @p n_shift as in leds_init(). */
void alert_actuation_init(uint8_t n_shift);

/**
 * @brief Drive the actuators for alert @p a at @p now_ms. Call every loop iteration
 *        so blinking stays smooth.
 * @param tps_pct Throttle 0-100 % for the shift light (ignored if no shift LEDs).
 */
void alert_actuation_update(const alert_t *a, float tps_pct, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_ACTUATION_H */
