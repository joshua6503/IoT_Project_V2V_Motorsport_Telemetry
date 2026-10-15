/**
 * @file alert_logic.h
 * @brief Pure edge logic: decide whether a telemetry sample raises an alert.
 *
 * Hardware-independent (no SDK, no RTOS, no I/O) so it runs unchanged on the
 * RW612, the ESP32 and the host PC (unit tests in tests/unit/).
 *
 * The evaluator is stateful (overtemp hysteresis, brake rise rate, hard-brake
 * latch): keep one alert_ctx_t per node and feed samples in time order.
 * All thresholds live in alert_config_t; see alert_config_default().
 */
#ifndef ALERT_LOGIC_H
#define ALERT_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "v2v_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bits of telemetry_t::sensor_ok: set when that sensor gave a valid reading. */
#define ALERT_SENSOR_DIST  0x01u
#define ALERT_SENSOR_TEMP  0x02u
#define ALERT_SENSOR_BRAKE 0x04u
/** Sensors that must be valid; a missing bit raises alert_t::sensor_fault. */
#define ALERT_SENSOR_REQUIRED (ALERT_SENSOR_DIST | ALERT_SENSOR_TEMP | ALERT_SENSOR_BRAKE)

/** One telemetry sample, units as in interfaces/payload.schema.json. */
typedef struct
{
    uint32_t ts_ms;       /**< Sample time, node uptime in ms. */
    float speed_kmh;
    float accel_g_x;      /**< Longitudinal axis: TBD, depends on IMU mounting. */
    float accel_g_y;
    float accel_g_z;
    float temp_c;
    float distance_cm;    /**< Negative when the sensor did not answer (then clear ALERT_SENSOR_DIST). */
    float brake_pct;      /**< Brake pressure, 0-100 %. Not in the JSON schema yet (OPEN-012). */
    float tps_pct;        /**< Throttle position, 0-100 %. Not in the JSON schema yet (OPEN-012). */
    uint8_t sensor_ok;    /**< ALERT_SENSOR_* bitmask. */
} telemetry_t;

/** Result of an evaluation. */
typedef struct
{
    v2v_alert_type_t type;
    v2v_severity_t severity;
    bool sensor_fault;    /**< A required ALERT_SENSOR_* bit is missing from sensor_ok. */
} alert_t;

/** Alert thresholds. Values come from alert_config_default() or the caller. */
typedef struct
{
    float obstacle_warn_cm;      /**< distance < this -> OBSTACLE WARNING */
    float obstacle_crit_cm;      /**< distance < this -> OBSTACLE CRITICAL */
    float temp_on_c;             /**< temp >= this latches OVERTEMP WARNING */
    float temp_off_c;            /**< temp <= this releases the latch */
    float temp_crit_c;           /**< temp >= this -> OVERTEMP CRITICAL */
    float brake_hard_pct;        /**< brake level needed for HARD_BRAKE */
    float brake_rate_pct_per_s;  /**< brake rise rate needed for HARD_BRAKE */
    uint32_t hard_brake_hold_ms; /**< HARD_BRAKE stays latched this long */
} alert_config_t;

/** Evaluator context: configuration plus the state carried between samples. */
typedef struct
{
    alert_config_t cfg;
    bool has_prev_brake;
    float prev_brake_pct;
    uint32_t prev_brake_ts_ms;
    bool overtemp_on;
    bool hard_brake_active;
    uint32_t hard_brake_until_ms;
} alert_ctx_t;

/**
 * @brief Fill @p cfg with the MP1 bench defaults (OPEN-005: to calibrate).
 * @param[out] cfg Must not be NULL (ignored if NULL).
 */
void alert_config_default(alert_config_t *cfg);

/**
 * @brief Reset @p ctx and copy @p cfg into it.
 * @param[out] ctx Must not be NULL (ignored if NULL).
 * @param[in]  cfg Thresholds; NULL uses alert_config_default().
 */
void alert_init(alert_ctx_t *ctx, const alert_config_t *cfg);

/**
 * @brief Evaluate one sample against the alert rules.
 *
 * Rules (thresholds from ctx->cfg):
 * - OBSTACLE: valid distance < obstacle_crit_cm -> CRITICAL, < obstacle_warn_cm -> WARNING.
 *   A negative distance or a missing ALERT_SENSOR_DIST bit raises no OBSTACLE alert.
 * - OVERTEMP: WARNING latches at temp >= temp_on_c and releases at temp <= temp_off_c;
 *   temp >= temp_crit_c -> CRITICAL. An invalid temperature releases the latch.
 * - HARD_BRAKE: brake >= brake_hard_pct and rise rate since the previous valid brake
 *   sample >= brake_rate_pct_per_s -> CRITICAL, latched for hard_brake_hold_ms.
 * - Arbitration (DEC-003): highest severity wins; tie-break HARD_BRAKE > OBSTACLE > OVERTEMP.
 * - sensor_fault = any ALERT_SENSOR_REQUIRED bit missing from sensor_ok (DEC-008); it does
 *   not suppress valid alerts from other sensors.
 *
 * @param[in,out] ctx     Context from alert_init(). Must not be NULL.
 * @param[in]     sample  Telemetry sample. Must not be NULL.
 * @param[out]    out     Filled with the result (V2V_ALERT_NONE / INFO if none). Must not be NULL.
 * @return true if out->type != V2V_ALERT_NONE; false otherwise or on NULL arguments.
 */
bool alert_evaluate(alert_ctx_t *ctx, const telemetry_t *sample, alert_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_LOGIC_H */
