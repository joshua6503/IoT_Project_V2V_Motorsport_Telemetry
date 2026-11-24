/**
 * @file alert_actuation.c
 * @brief Actuation service. See alert_actuation.h.
 */
#include "alert_actuation.h"

#include "alert_cmd.h"
#include "alert_pattern.h"
#include "buzzer.h"
#include "leds.h"

/* Touched by one task only (the one that calls alert_actuation_update); initialised before the scheduler starts. */
static alert_remote_t s_remote;

void alert_actuation_init(uint8_t n_shift)
{
    leds_init(n_shift);
    buzzer_init();
    alert_remote_init(&s_remote);
}

void alert_actuation_remote(const alert_cmd_t *cmd, uint32_t now_ms)
{
    alert_remote_apply(&s_remote, cmd, now_ms);
}

bool alert_actuation_remote_active(v2v_severity_t *severity)
{
    if (severity != NULL)
    {
        *severity = s_remote.severity;
    }
    return s_remote.active;
}

void alert_actuation_update(const alert_t *a, float tps_pct, uint32_t now_ms)
{
    /* A remote alert (DEC-013) only shows when it is strictly more severe than the local one. */
    alert_t eff       = alert_remote_merge(&s_remote, a, now_ms);
    alert_pattern_t p = alert_pattern_for(&eff, now_ms);
    leds_set_status(p.led_green, p.led_yellow, p.led_red);
    buzzer_set(p.buzzer_on);

    if (leds_shift_count() > 0u)
    {
        leds_set_shift(alert_shift_level(tps_pct, leds_shift_count()));
    }
}
