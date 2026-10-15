/**
 * @file pot.h
 * @brief Potentiometer on an ADC input, scaled to 0-100 %.
 */
#ifndef POT_H
#define POT_H

#include <stdbool.h>
#include <stdint.h>

#include "board_io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    board_adc_t ch;
    int32_t raw_min; /**< raw reading at 0 % */
    int32_t raw_max; /**< raw reading at 100 % (may be < raw_min for a reversed pot) */
} pot_t;

/** Pure scaling: raw -> 0..100 %, clamped. raw_min == raw_max gives 0. */
float pot_raw_to_pct(int32_t raw, int32_t raw_min, int32_t raw_max);

/**
 * @brief Read @p pot once.
 * @param[out] pct 0-100 %.
 * @param[out] raw Raw ADC counts, for bench calibration (may be NULL).
 * @return false if the ADC read failed.
 */
bool pot_read(const pot_t *pot, float *pct, int32_t *raw);

#ifdef __cplusplus
}
#endif

#endif /* POT_H */
