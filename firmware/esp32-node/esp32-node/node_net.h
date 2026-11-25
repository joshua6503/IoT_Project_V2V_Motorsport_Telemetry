/**
 * node_net.h - Wi-Fi station + MQTT client (ESP-IDF esp-mqtt bundled in the core).
 *
 * Non-blocking: net_loop() only checks timers; the MQTT I/O runs in the esp-mqtt
 * task. Publishing never waits on the network. MQTT is informative: when the
 * link is down, messages are dropped (and logged), never queued for later.
 */
#ifndef NODE_NET_H
#define NODE_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "src/v2v_protocol.h"

/** Longest node_id / origin / source_node string, pattern ^[a-z0-9-]{1,32}$ (+ NUL). */
#define NET_ID_MAX 33u

/** Remote command received on v2v/<node>/cmd or v2v/all/cmd (DEC-013), already validated. */
typedef enum
{
    NET_CMD_ALERT = 1,
    NET_CMD_CLEAR = 2
} net_cmd_kind_t;

typedef struct
{
    net_cmd_kind_t   kind;
    uint32_t         seq;
    char             origin[NET_ID_MAX];
    v2v_alert_type_t alert_type;  /**< NET_CMD_ALERT only */
    v2v_severity_t   severity;    /**< NET_CMD_ALERT only */
    uint32_t         duration_ms; /**< NET_CMD_ALERT only, 100..10000 */
    char             source_node[NET_ID_MAX]; /**< "" when absent */
} net_cmd_t;

/** Pop the next validated, de-duplicated command. false when none is pending. Never blocks. */
bool net_cmd_poll(net_cmd_t *out);

/** Start Wi-Fi (auto reconnect) and create the MQTT client. Call once from setup(). */
void net_init(void);

/** Housekeeping: Wi-Fi watchdog, MQTT start/reconnect with backoff. Call every loop. */
void net_loop(uint32_t now_ms);

bool net_wifi_connected(void);
bool net_mqtt_connected(void);

/** Telemetry, QoS 0. Returns false if not sent (offline / outbox backlog). */
bool net_publish_telemetry(const char *json, int len);

/** Per-variable topic v2v/<node>/sensor/brake_pct: plain text (e.g. "42.5" or "null"), QoS 0, not
 *  retained. Shares the telemetry publish task and queue; never blocks. Returns false if dropped. */
bool net_publish_sensor_brake(const char *txt, int len);

/** Fill @p buf with link/publish diagnostics (BSSID, channel, RSSI, longest publish since the
 *  last call, publish ok/fail counters, last MQTT transport errno). Resets the max. */
void net_diag_str(char *buf, size_t size);

/** Alert, QoS 1, not retained. Returns false if not sent (offline / outbox full). */
bool net_publish_alert(const char *json, int len);

#endif /* NODE_NET_H */
