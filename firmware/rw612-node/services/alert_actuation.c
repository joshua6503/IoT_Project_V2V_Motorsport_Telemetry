/**
 * @file alert_actuation.c
 * @brief Actuation service. See alert_actuation.h.
 */
#include "alert_actuation.h"

#include "alert_pattern.h"
#include "buzzer.h"
#include "leds.h"

void alert_actuation_init(uint8_t n_shift)
{
    leds_init(n_shift);
    buzzer_init();
}

void alert_actuation_update(const alert_t *a, float tps_pct, uint32_t now_ms)
{
    alert_pattern_t p = alert_pattern_for(a, now_ms);
    leds_set_status(p.led_green, p.led_yellow, p.led_red);
    buzzer_set(p.buzzer_on);

    if (leds_shift_count() > 0u)
    {
        leds_set_shift(alert_shift_level(tps_pct, leds_shift_count()));
    }
}
