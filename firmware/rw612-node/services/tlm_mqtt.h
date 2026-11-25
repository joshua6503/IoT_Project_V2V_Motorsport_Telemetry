/**
 * @file tlm_mqtt.h
 * @brief Wi-Fi join + MQTT client service for the RW612 node (phase F3).
 *
 * Wraps the SDK's WPL (Wi-Fi) and lwIP MQTT client behind a small API so the app layer
 * never touches them. Topics: v2v/{node_id}/telemetry | alert | status (interfaces/mqtt-topics.md).
 * The credentials arrive in a tlm_mqtt_config_t built by the app from net_config.h (git-ignored);
 * this module never prints the Wi-Fi passphrase or the MQTT password.
 *
 * Threading: tlm_mqtt_wifi_join(), tlm_mqtt_start(), tlm_mqtt_service() and the publish
 * functions are called from ONE task (the "tlm" task). The MQTT callbacks run on the lwIP
 * tcpip thread; the counters they update are plain volatile 32-bit values.
 * No board time functions are used here (board_time_* is not reentrant, see main_mp2.c).
 */
#ifndef TLM_MQTT_H
#define TLM_MQTT_H

#include <stdbool.h>
#include <stdint.h>

#include "alert_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Immutable configuration; all strings must outlive the module (use string literals). */
typedef struct
{
    const char *node_id;      /**< e.g. "rw612-a"; also the MQTT client id. */
    const char *wifi_ssid;
    const char *wifi_pass;
    const char *broker_ip;    /**< dotted IPv4 literal; no DNS (the demo broker has a fixed IP). */
    uint16_t broker_port;
    const char *mqtt_user;    /**< NULL or "" = no user name. */
    const char *mqtt_pass;    /**< NULL or "" = no password. */
    uint16_t keepalive_s;     /**< MQTT keep-alive; the broker declares the node dead after 1.5x. */
    bool wifi_power_save_off; /**< true: switch the driver's IEEE + deep-sleep power save off after each join. */
} tlm_mqtt_config_t;

/** Per-variable topics v2v/{node_id}/sensor/{brake_pct,temp_c,distance_cm} (plain text, QoS 0, not retained). */
typedef enum
{
    TLM_SENSOR_BRAKE_PCT = 0,
    TLM_SENSOR_TEMP_C,
    TLM_SENSOR_DISTANCE_CM,
    TLM_SENSOR_COUNT
} tlm_mqtt_sensor_t;

/** Counters for the periodic log line and the priority-fallback measurement. */
typedef struct
{
    uint32_t wifi_drops;       /**< Wi-Fi link lost events. */
    uint32_t mqtt_drops;       /**< MQTT connections lost after having been established. */
    uint32_t mqtt_reconnects;  /**< MQTT connections established after the first one. */
    uint32_t pub_ok;
    uint32_t pub_err;          /**< mqtt_publish() refused (buffer / in-flight table full) or not connected. */
    uint32_t sens_ok;          /**< Per-sensor topic publishes accepted (already counted in pub_ok). */
    uint32_t sens_err;         /**< Per-sensor topic publishes refused (already counted in pub_err). */
    uint32_t cmd_rx;           /**< Complete messages received on the cmd topics (valid or not). */
    uint32_t cmd_q_drop;       /**< Received commands lost because the hand-over queue was full. */
    uint32_t sub_ok;           /**< Number of the two cmd subscriptions acknowledged in this session (0..2). */
} tlm_mqtt_stats_t;

/** One message received on v2v/{node_id}/cmd or v2v/all/cmd, already parsed (DEC-013). */
typedef struct
{
    alert_cmd_status_t status; /**< ALERT_CMD_OK, or why it must be ignored (then @c cmd is zeroed). */
    bool broadcast;            /**< true: arrived on v2v/all/cmd. */
    alert_cmd_t cmd;
} tlm_mqtt_cmd_t;

/**
 * @brief Initialise the Wi-Fi driver and block until the node has joined the access point and has an IP.
 *
 * Retries every 5 s forever (the sensing task keeps running); prints the result codes, never the passphrase.
 * @return false only if the configuration is invalid or the Wi-Fi driver cannot start.
 */
bool tlm_mqtt_wifi_join(const tlm_mqtt_config_t *cfg);

/** Create the MQTT client and start connecting (LWT registered, retained "online" on connect). */
bool tlm_mqtt_start(void);

/** Call every ~200 ms: re-joins the access point if the link has been down for too long. */
void tlm_mqtt_service(void);

/** True while the MQTT session is established. */
bool tlm_mqtt_connected(void);

/** Wi-Fi link state as last reported by the driver. */
bool tlm_mqtt_wifi_up(void);

/** Last data-frame signal level in dBm as reported by the Wi-Fi driver (0 if unknown). */
int tlm_mqtt_wifi_rssi(void);

/** Current lwIP address of the default netif as text plus its flags ("ip=a.b.c.d up=1 link=1"). */
void tlm_mqtt_netif_text(char *buf, unsigned n);

/** QoS 0, not retained, to v2v/{node_id}/telemetry. @p json is NUL-free and @p len bytes long. */
bool tlm_mqtt_publish_telemetry(const char *json, uint16_t len);

/** QoS 0, not retained, to v2v/{node_id}/sensor/{name}. @p text is the output of tlm_json_value1(). */
bool tlm_mqtt_publish_sensor(tlm_mqtt_sensor_t which, const char *text, uint16_t len);

/** QoS 1, not retained, to v2v/{node_id}/alert. */
bool tlm_mqtt_publish_alert(const char *json, uint16_t len);

/**
 * Next received command, if any (non-blocking). Commands arrive on v2v/{node_id}/cmd and v2v/all/cmd
 * (QoS 1, not retained); the module re-subscribes on every MQTT (re)connect. Parsing is pure
 * (alert_cmd_parse) and runs on the lwIP thread; the tlm task applies de-duplication and logging.
 * Receiving a command never publishes anything.
 */
bool tlm_mqtt_cmd_poll(tlm_mqtt_cmd_t *out);

/** Snapshot of the counters. */
void tlm_mqtt_get_stats(tlm_mqtt_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TLM_MQTT_H */
