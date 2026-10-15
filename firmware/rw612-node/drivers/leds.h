/**
 * @file leds.h
 * @brief Status LEDs (green / yellow / red) and optional shift-light bar.
 */
#ifndef LEDS_H
#define LEDS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure the LED outputs, all off.
 * @param n_shift Shift-light LEDs to use (APP_SHIFT_LEDS); clamped to the board count, may be 0.
 */
void leds_init(uint8_t n_shift);

/** Set the three status LEDs (true = lit, polarity handled here). */
void leds_set_status(bool green, bool yellow, bool red);

/** Light the first @p level shift LEDs, the rest off. */
void leds_set_shift(uint8_t level);

/** Shift LEDs in use after clamping. */
uint8_t leds_shift_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LEDS_H */
