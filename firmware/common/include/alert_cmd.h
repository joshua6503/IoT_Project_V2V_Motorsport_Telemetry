/**
 * @file alert_cmd.h
 * @brief Pure logic for dashboard -> node commands (DEC-013): parser, (origin, seq) de-duplication and
 *        the "remote alert" that is merged with the local alert.
 *
 * Hardware-independent (no SDK, no RTOS, no I/O, no allocation). Topics are
 * v2v/{node_id}/cmd and v2v/all/cmd, QoS 1, not retained. Payloads (flat JSON objects):
 *
 *   {"cmd":"alert","seq":<uint32>,"origin":"dashboard","alert_type":"manual|hard_brake|obstacle|overtemp",
 *    "severity":"info|warning|critical","duration_ms":<100..10000, default 3000>,"source_node":"<optional>"}
 *   {"cmd":"clear","seq":<uint32>,"origin":"dashboard"}
 *
 * The parser is tolerant on receive: unknown extra keys are ignored. Everything it does accept is
 * range-checked; values out of range are rejected, never clamped.
 */
#ifndef ALERT_CMD_H
#define ALERT_CMD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "alert_logic.h"
#include "v2v_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ALERT_CMD_MAX_LEN          256u  /**< Longest payload parsed; longer is ERR_TOO_LONG. */
#define ALERT_CMD_ID_MAX_LEN       32u   /**< origin / source_node: ^[a-z0-9-]{1,32}$ */
#define ALERT_CMD_DURATION_MIN_MS  100u
#define ALERT_CMD_DURATION_MAX_MS  10000u
#define ALERT_CMD_DURATION_DEFAULT_MS 3000u
#define ALERT_CMD_DEDUPE_SLOTS     16u

typedef enum
{
    ALERT_CMD_NONE = 0,
    ALERT_CMD_ALERT,
    ALERT_CMD_CLEAR
} alert_cmd_kind_t;

typedef enum
{
    ALERT_CMD_OK = 0,
    ALERT_CMD_ERR_JSON,        /**< Not a flat JSON object (syntax, nesting, bad escape, trailing bytes, NULL/empty). */
    ALERT_CMD_ERR_TOO_LONG,    /**< Payload longer than ALERT_CMD_MAX_LEN. */
    ALERT_CMD_ERR_UNKNOWN_CMD, /**< "cmd" present but not "alert" / "clear" (or not a string). */
    ALERT_CMD_ERR_MISSING,     /**< A required field is absent. */
    ALERT_CMD_ERR_RANGE,       /**< A field has the wrong type or is out of range / not in its enum. */
    ALERT_CMD_ERR_DUP_KEY      /**< A known key appears twice. */
} alert_cmd_status_t;

typedef struct
{
    alert_cmd_kind_t kind;
    uint32_t seq;
    char origin[ALERT_CMD_ID_MAX_LEN + 1u];
    /* alert only */
    v2v_alert_type_t alert_type;
    v2v_severity_t severity;
    uint32_t duration_ms;
    char source_node[ALERT_CMD_ID_MAX_LEN + 1u]; /**< "" when absent. */
} alert_cmd_t;

/** Short name of a status, for logs ("OK", "ERR_JSON", ...). */
const char *alert_cmd_status_name(alert_cmd_status_t s);

/**
 * @brief Parse one command payload.
 * @param buf Payload bytes (need not be NUL-terminated).
 * @param len Length in bytes.
 * @param[out] out Filled on ALERT_CMD_OK; zeroed first in every case. Must not be NULL.
 */
alert_cmd_status_t alert_cmd_parse(const char *buf, size_t len, alert_cmd_t *out);

/** Last ALERT_CMD_DEDUPE_SLOTS (origin, seq) pairs, oldest evicted first. */
typedef struct
{
    char origin[ALERT_CMD_DEDUPE_SLOTS][ALERT_CMD_ID_MAX_LEN + 1u];
    uint32_t seq[ALERT_CMD_DEDUPE_SLOTS];
    bool used[ALERT_CMD_DEDUPE_SLOTS];
    uint8_t next;
} alert_cmd_dedupe_t;

void alert_cmd_dedupe_init(alert_cmd_dedupe_t *d);

/** @return true if (origin, seq) was already seen; otherwise records it and returns false. */
bool alert_cmd_dedupe_seen(alert_cmd_dedupe_t *d, const alert_cmd_t *cmd);

/** Alert imposed by a command: active until @c until_ms (cap ALERT_CMD_DURATION_MAX_MS). */
typedef struct
{
    bool active;
    v2v_alert_type_t type;
    v2v_severity_t severity;
    uint32_t until_ms;
} alert_remote_t;

void alert_remote_init(alert_remote_t *r);

/** "alert" starts (or replaces) the remote alert for duration_ms from @p now_ms; "clear" ends it. */
void alert_remote_apply(alert_remote_t *r, const alert_cmd_t *cmd, uint32_t now_ms);

/**
 * @brief Alert to actuate: the remote one when it is active, not expired and strictly more severe than
 *        @p local (or local is NONE); otherwise @p local. sensor_fault always comes from @p local.
 *        Expiry is wrap-safe and is recorded in @p r.
 */
alert_t alert_remote_merge(alert_remote_t *r, const alert_t *local, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* ALERT_CMD_H */
