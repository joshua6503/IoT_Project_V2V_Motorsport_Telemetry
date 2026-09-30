/**
 * config.example.h - template for config.h (secrets and per-device settings).
 *
 * Copy to config.h and fill in. config.h is git-ignored: never commit it.
 *   Copy-Item config.example.h config.h
 */
#ifndef CONFIG_H
#define CONFIG_H

/* Wi-Fi */
#define WIFI_SSID     "TBD_SSID"
#define WIFI_PASSWORD "TBD_PASSWORD"

/* MQTT broker (the PC running gateway/docker-compose.yml) */
#define MQTT_HOST "192.168.TBD.TBD"
#define MQTT_PORT 1883

/* Identity - must match interfaces/README.md (identifier mapping) */
#define NODE_ID_STR "esp32-b"
#define NODE_ID_NUM 0x0002u

/* Pins - TBD, see hardware/pinout.md. Do not guess. */
/* #define PIN_BUTTON   TBD */
/* #define PIN_LED      TBD */
/* #define PIN_BUZZER   TBD */

#endif /* CONFIG_H */
