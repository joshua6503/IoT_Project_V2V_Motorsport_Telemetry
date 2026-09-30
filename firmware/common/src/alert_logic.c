/**
 * @file alert_logic.c
 * @brief Stub implementation of the alert rules. See alert_logic.h.
 */
#include "alert_logic.h"

#include <stddef.h>

bool alert_evaluate(const telemetry_t *sample, alert_t *out)
{
    if ((sample == NULL) || (out == NULL))
    {
        return false;
    }

    out->type     = V2V_ALERT_NONE;
    out->severity = V2V_SEVERITY_INFO;

    /* TODO(M3): hard_brake / obstacle / overtemp rules. Thresholds are TBD and
     * must come from requirements.md, not be guessed here. */
    return false;
}
