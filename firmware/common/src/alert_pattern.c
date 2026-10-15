/**
 * @file alert_pattern.c
 * @brief DEC-004 actuator patterns and shift-light level. See alert_pattern.h.
 */
#include "alert_pattern.h"

#include <stddef.h>

alert_pattern_t alert_pattern_for(const alert_t *a, uint32_t now_ms)
{
    alert_pattern_t p = {false, false, false, false};

    bool active = (a != NULL) && (a->type != V2V_ALERT_NONE);

    if (active && (a->severity == V2V_SEVERITY_CRITICAL))
    {
        bool on     = (now_ms % ALERT_PATTERN_CRIT_PERIOD_MS) < ALERT_PATTERN_CRIT_ON_MS;
        p.led_red   = on;
        p.buzzer_on = on;
    }
    else if (active && (a->severity == V2V_SEVERITY_WARNING))
    {
        p.led_yellow = true;
        p.buzzer_on  = (now_ms % ALERT_PATTERN_WARN_PERIOD_MS) < ALERT_PATTERN_WARN_BEEP_MS;
    }
    else if ((a != NULL) && a->sensor_fault)
    {
        bool first_half = (now_ms % ALERT_PATTERN_FAULT_PERIOD_MS) < (ALERT_PATTERN_FAULT_PERIOD_MS / 2u);
        p.led_red       = first_half;
        p.led_yellow    = !first_half;
    }
    else
    {
        p.led_green = true;
    }
    return p;
}

uint8_t alert_shift_level(float tps_pct, uint8_t n_leds)
{
    if ((n_leds == 0u) || !(tps_pct > 0.0f)) /* also catches NaN */
    {
        return 0u;
    }
    if (tps_pct >= 100.0f)
    {
        return n_leds;
    }
    return (uint8_t)((tps_pct * (float)n_leds) / 100.0f);
}
