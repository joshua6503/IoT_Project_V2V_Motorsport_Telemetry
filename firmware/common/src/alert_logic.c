/**
 * @file alert_logic.c
 * @brief Alert rules (obstacle, overtemp, hard brake) and arbitration. See alert_logic.h.
 */
#include "alert_logic.h"

#include <stddef.h>

/** Wrap-safe "a is at or after b" for uint32_t millisecond timestamps. */
static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void alert_config_default(alert_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }
    cfg->obstacle_warn_cm     = 30.0f;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->obstacle_crit_cm     = 15.0f;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->temp_on_c            = 30.0f;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->temp_off_c           = 28.0f;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->temp_crit_c          = 35.0f;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->brake_hard_pct       = 70.0f;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->brake_rate_pct_per_s = 150.0f; /* ASSUMPTION (bench default, calibrate — OPEN-005) */
    cfg->hard_brake_hold_ms   = 2000u;  /* ASSUMPTION (bench default, calibrate — OPEN-005) */
}

void alert_init(alert_ctx_t *ctx, const alert_config_t *cfg)
{
    if (ctx == NULL)
    {
        return;
    }
    if (cfg != NULL)
    {
        ctx->cfg = *cfg;
    }
    else
    {
        alert_config_default(&ctx->cfg);
    }
    ctx->has_prev_brake      = false;
    ctx->prev_brake_pct      = 0.0f;
    ctx->prev_brake_ts_ms    = 0u;
    ctx->overtemp_on         = false;
    ctx->hard_brake_active   = false;
    ctx->hard_brake_until_ms = 0u;
}

static v2v_severity_t eval_obstacle(const alert_ctx_t *ctx, const telemetry_t *s)
{
    if (((s->sensor_ok & ALERT_SENSOR_DIST) == 0u) || (s->distance_cm < 0.0f))
    {
        return V2V_SEVERITY_INFO;
    }
    if (s->distance_cm < ctx->cfg.obstacle_crit_cm)
    {
        return V2V_SEVERITY_CRITICAL;
    }
    if (s->distance_cm < ctx->cfg.obstacle_warn_cm)
    {
        return V2V_SEVERITY_WARNING;
    }
    return V2V_SEVERITY_INFO;
}

static v2v_severity_t eval_overtemp(alert_ctx_t *ctx, const telemetry_t *s)
{
    if ((s->sensor_ok & ALERT_SENSOR_TEMP) == 0u)
    {
        ctx->overtemp_on = false;
        return V2V_SEVERITY_INFO;
    }
    if (s->temp_c >= ctx->cfg.temp_on_c)
    {
        ctx->overtemp_on = true;
    }
    else if (s->temp_c <= ctx->cfg.temp_off_c)
    {
        ctx->overtemp_on = false;
    }
    else
    {
        /* Inside the hysteresis band: keep the previous state. */
    }

    if (s->temp_c >= ctx->cfg.temp_crit_c)
    {
        return V2V_SEVERITY_CRITICAL;
    }
    return ctx->overtemp_on ? V2V_SEVERITY_WARNING : V2V_SEVERITY_INFO;
}

static v2v_severity_t eval_hard_brake(alert_ctx_t *ctx, const telemetry_t *s)
{
    if ((s->sensor_ok & ALERT_SENSOR_BRAKE) != 0u)
    {
        if (ctx->has_prev_brake && (s->ts_ms != ctx->prev_brake_ts_ms))
        {
            float dt_s = (float)(uint32_t)(s->ts_ms - ctx->prev_brake_ts_ms) / 1000.0f;
            float rate = (s->brake_pct - ctx->prev_brake_pct) / dt_s;
            if ((s->brake_pct >= ctx->cfg.brake_hard_pct) && (rate >= ctx->cfg.brake_rate_pct_per_s))
            {
                ctx->hard_brake_active   = true;
                ctx->hard_brake_until_ms = s->ts_ms + ctx->cfg.hard_brake_hold_ms;
            }
        }
        ctx->has_prev_brake   = true;
        ctx->prev_brake_pct   = s->brake_pct;
        ctx->prev_brake_ts_ms = s->ts_ms;
    }
    else
    {
        /* No valid reading: the next valid one has no reference for the rate. */
        ctx->has_prev_brake = false;
    }

    if (ctx->hard_brake_active && time_reached(s->ts_ms, ctx->hard_brake_until_ms))
    {
        ctx->hard_brake_active = false;
    }
    return ctx->hard_brake_active ? V2V_SEVERITY_CRITICAL : V2V_SEVERITY_INFO;
}

bool alert_evaluate(alert_ctx_t *ctx, const telemetry_t *sample, alert_t *out)
{
    if ((ctx == NULL) || (sample == NULL) || (out == NULL))
    {
        return false;
    }

    /* Candidates in DEC-003 tie-break order: earlier entry wins on equal severity. */
    const v2v_alert_type_t types[3] = {V2V_ALERT_HARD_BRAKE, V2V_ALERT_OBSTACLE, V2V_ALERT_OVERTEMP};
    v2v_severity_t sev[3];
    sev[0] = eval_hard_brake(ctx, sample);
    sev[1] = eval_obstacle(ctx, sample);
    sev[2] = eval_overtemp(ctx, sample);

    out->type     = V2V_ALERT_NONE;
    out->severity = V2V_SEVERITY_INFO;
    for (size_t i = 0u; i < 3u; i++)
    {
        if (sev[i] > out->severity)
        {
            out->type     = types[i];
            out->severity = sev[i];
        }
    }

    /* DEC-008: the fault comes only from the sensor_ok bits; the caller clears
     * ALERT_SENSOR_DIST when the distance sensor did not answer. */
    out->sensor_fault = (sample->sensor_ok & ALERT_SENSOR_REQUIRED) != ALERT_SENSOR_REQUIRED;

    return out->type != V2V_ALERT_NONE;
}
