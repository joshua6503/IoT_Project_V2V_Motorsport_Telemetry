/**
 * @file net_config.example.h
 * @brief Template for app/net_config.h (git-ignored). Copy it, then fill in YOUR values:
 *
 *     Copy-Item firmware\rw612-node\app\net_config.example.h firmware\rw612-node\app\net_config.h
 *
 * Never commit net_config.h and never paste its values in chats, logs or docs.
 */
#ifndef NET_CONFIG_H
#define NET_CONFIG_H

#define NET_WIFI_SSID       "your-2.4GHz-ssid"
#define NET_WIFI_PASSPHRASE "your-wifi-passphrase"

/** IPv4 literal of the machine running Mosquitto (no DNS lookup is done). */
#define NET_MQTT_BROKER_IP "192.168.0.10"
#define NET_MQTT_PORT      1883u

/** The Pi broker has no anonymous access: set both. Empty strings = anonymous. */
#define NET_MQTT_USER "v2v-node"
#define NET_MQTT_PASS "your-mqtt-password"

#endif /* NET_CONFIG_H */
