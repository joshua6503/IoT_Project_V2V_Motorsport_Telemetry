/**
 * node_io.h - local I/O of node B: brake pot, BOOT button, LEDs and buzzer.
 * Everything is non-blocking; time is passed in (millis()).
 */
#ifndef NODE_IO_H
#define NODE_IO_H

#include <stdbool.h>
#include <stdint.h>

#include "src/alert_pattern.h"

/** Configure pins and the ADC. Call once from setup(). */
void io_init(void);

/** Averaged wiper voltage in mV (ADC1, calibrated by the core). */
uint32_t io_read_brake_mv(void);

/** true if the wiper voltage is inside the plausible window (not shorted/open). */
bool io_brake_mv_valid(uint32_t mv);

/** Linear map of the wiper voltage to 0-100 % (BRAKE_MV_MIN..MAX, clamped). */
float io_brake_pct_from_mv(uint32_t mv);

/** true once per debounced BOOT press (falling edge). */
bool io_button_pressed(uint32_t now_ms);

/** Start the node-local confirmation of a manual alert (yellow blink + beep). */
void io_manual_feedback(uint32_t now_ms);

/**
 * Drive the LEDs and buzzer: @p p from alert_pattern_for(), with the manual
 * confirmation overlaid while it is active. Call every loop iteration.
 */
void io_apply(const alert_pattern_t *p, uint32_t now_ms);

#endif /* NODE_IO_H */
