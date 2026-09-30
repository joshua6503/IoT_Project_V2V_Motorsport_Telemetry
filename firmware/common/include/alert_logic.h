/**
 * @file alert_logic.h
 * @brief Pure edge logic: decide whether a telemetry sample raises a V2V alert.
 *
 * Hardware-independent (no SDK, no RTOS, no I/O) so it runs unchanged on the
 * RW612, the ESP32 and the host PC (unit tests in tests/unit/).
 */
#ifndef ALERT_LOGIC_H
#define ALERT_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "v2v_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One telemetry sample, units as in interfaces/payload.schema.json. */
typedef struct
{
    uint32_t ts_ms;       /**< Sample time, node uptime in ms. */
    float speed_kmh;
    float accel_g_x;      /**< Longitudinal axis: TBD, depends on IMU mounting. */
    float accel_g_y;
    float accel_g_z;
    float temp_c;
    float distance_cm;    /**< Negative when no echo / out of range. */
} telemetry_t;

/** Result of an evaluation. */
typedef struct
{
    v2v_alert_type_t type;
    v2v_severity_t severity;
} alert_t;

/**
 * @brief Evaluate one sample against the alert rules.
 *
 * STUB: thresholds are TBD (see docs/01-requirements/requirements.md), so the
 * current implementation never raises an alert.
 *
 * @param[in]  sample  Telemetry sample. Must not be NULL.
 * @param[out] out     Filled with the alert (V2V_ALERT_NONE if none). Must not be NULL.
 * @return true if an alert must be sent, false otherwise or on invalid arguments.
 */
bool alert_evaluate(const telemetry_t *sample, alert_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_LOGIC_H */
