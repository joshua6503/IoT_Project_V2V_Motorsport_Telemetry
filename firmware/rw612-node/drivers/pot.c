/**
 * @file pot.c
 * @brief Potentiometer driver. See pot.h.
 */
#include "pot.h"

#include <stddef.h>

float pot_raw_to_pct(int32_t raw, int32_t raw_min, int32_t raw_max)
{
    if (raw_max == raw_min)
    {
        return 0.0f;
    }
    float pct = ((float)(raw - raw_min) * 100.0f) / (float)(raw_max - raw_min);
    if (pct < 0.0f)
    {
        return 0.0f;
    }
    if (pct > 100.0f)
    {
        return 100.0f;
    }
    return pct;
}

bool pot_read(const pot_t *pot, float *pct, int32_t *raw)
{
    int32_t r;
    if ((pot == NULL) || (pct == NULL) || !board_adc_read_raw(pot->ch, &r))
    {
        return false;
    }
    *pct = pot_raw_to_pct(r, pot->raw_min, pot->raw_max);
    if (raw != NULL)
    {
        *raw = r;
    }
    return true;
}
