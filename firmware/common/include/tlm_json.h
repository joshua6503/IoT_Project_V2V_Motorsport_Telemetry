/**
 * @file tlm_json.h
 * @brief Pure-C JSON formatter for the MQTT payloads of interfaces/mqtt-topics.md.
 *
 * No SDK, RTOS, I/O, malloc or printf: it runs unchanged on the RW612 (newlib-nano without
 * %f), the ESP32 and the host PC (unit tests in tests/unit/). Numbers with decimals are
 * written with one decimal using integer math.
 *
 * Every function returns the payload length (without the NUL), or -1 on a NULL argument, an
 * invalid node_id, an unknown enum value or a buffer too small. It never writes past
 * buf[n - 1]; on failure it leaves an empty string in buf (if n > 0).
 */
#ifndef TLM_JSON_H
#define TLM_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "v2v_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One telemetry sample (topic v2v/{node_id}/telemetry). */
typedef struct
{
    const char *node_id; /**< Must match ^[a-z0-9-]{1,32}$ (checked). */
    uint32_t ts_ms;
    uint32_t seq;
    float brake_pct;      /**< Always written; NaN is written as 0.0. */
    bool temp_valid;      /**< false (or a non-finite temp_c) -> "temp_c":null */
    float temp_c;
    bool dist_valid;      /**< false (or a non-finite distance_cm) -> "distance_cm":null */
    float distance_cm;
    bool has_tps;         /**< true -> optional "tps_pct" is written */
    float tps_pct;
    bool has_sensor_ok;   /**< true -> optional "sensor_ok" (ALERT_SENSOR_* bitmask) is written */
    uint8_t sensor_ok;
} tlm_json_telemetry_t;

/**
 * @brief {"node_id","ts_ms","seq","brake_pct","temp_c","distance_cm"[,"tps_pct"][,"sensor_ok"]}
 */
int tlm_json_telemetry(char *buf, size_t n, const tlm_json_telemetry_t *t);

/**
 * @brief {"node_id","ts_ms","seq","alert_type","severity"}. V2V_ALERT_NONE is rejected (-1).
 */
int tlm_json_alert(char *buf, size_t n, const char *node_id, uint32_t ts_ms, uint16_t seq, v2v_alert_type_t type,
                   v2v_severity_t severity);

/**
 * @brief {"node_id","state":"online"|"offline"} (status and LWT payload).
 */
int tlm_json_status(char *buf, size_t n, const char *node_id, bool online);

#ifdef __cplusplus
}
#endif

#endif /* TLM_JSON_H */
