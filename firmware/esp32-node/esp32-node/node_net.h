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

/** Start Wi-Fi (auto reconnect) and create the MQTT client. Call once from setup(). */
void net_init(void);

/** Housekeeping: Wi-Fi watchdog, MQTT start/reconnect with backoff. Call every loop. */
void net_loop(uint32_t now_ms);

bool net_wifi_connected(void);
bool net_mqtt_connected(void);

/** Telemetry, QoS 0. Returns false if not sent (offline / outbox backlog). */
bool net_publish_telemetry(const char *json, int len);

/** Fill @p buf with link/publish diagnostics (BSSID, channel, RSSI, longest publish since the
 *  last call, publish ok/fail counters, last MQTT transport errno). Resets the max. */
void net_diag_str(char *buf, size_t size);

/** Alert, QoS 1, not retained. Returns false if not sent (offline / outbox full). */
bool net_publish_alert(const char *json, int len);

#endif /* NODE_NET_H */
