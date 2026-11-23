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

/** Counters for the periodic log line and the priority-fallback measurement. */
typedef struct
{
    uint32_t wifi_drops;       /**< Wi-Fi link lost events. */
    uint32_t mqtt_drops;       /**< MQTT connections lost after having been established. */
    uint32_t mqtt_reconnects;  /**< MQTT connections established after the first one. */
    uint32_t pub_ok;
    uint32_t pub_err;          /**< mqtt_publish() refused (buffer / in-flight table full) or not connected. */
} tlm_mqtt_stats_t;

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

/** QoS 1, not retained, to v2v/{node_id}/alert. */
bool tlm_mqtt_publish_alert(const char *json, uint16_t len);

/** Snapshot of the counters. */
void tlm_mqtt_get_stats(tlm_mqtt_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* TLM_MQTT_H */
