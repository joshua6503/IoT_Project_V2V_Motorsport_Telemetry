/**
 * config.example.h - template for config.h (secrets and per-device settings).
 *
 * Copy to config.h and fill in. config.h is git-ignored: never commit it.
 *   Copy-Item config.example.h config.h
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ------------------------------------------------------------------------- */
/* Wi-Fi: the Raspberry Pi 5 access point (2.4 GHz, ARQUITECTURA §4)          */
/* ------------------------------------------------------------------------- */
#define WIFI_SSID     "V2V-PIT"
#define WIFI_PASSWORD "TBD_PASSWORD"

/* ------------------------------------------------------------------------- */
/* MQTT broker (Mosquitto on the Pi; use a laptop broker for bench tests)     */
/* ------------------------------------------------------------------------- */
#define MQTT_HOST     "192.168.TBD.TBD"
#define MQTT_PORT     1883
#define MQTT_USER     "TBD_MQTT_USER"
#define MQTT_PASSWORD "TBD_MQTT_PASSWORD"

/* Identity - must match interfaces/README.md (identifier mapping) */
#define NODE_ID_STR "esp32-b"
#define NODE_ID_NUM 0x0002u

/* ------------------------------------------------------------------------- */
/* Pins - see hardware/pinout.md (Node B) for the citations.                  */
/* ADC1 only: ADC2 cannot be used while Wi-Fi is on. Avoid strapping pins     */
/* (0, 2, 5, 12, 15), flash pins (6-11) and input-only pins for outputs.      */
/* ------------------------------------------------------------------------- */
#define PIN_BRAKE_POT   32 /* ADC1_CH4, pot wiper                              */
#define PIN_LED_GREEN   25 /* active high, 220 ohm to GND                      */
#define PIN_LED_YELLOW  26 /* active high, 220 ohm to GND                      */
#define PIN_LED_RED     27 /* active high, 220 ohm to GND                      */
#define PIN_BUZZER      23 /* NPN base through 1 kohm, active high             */
#define PIN_BUTTON       0 /* on-board BOOT button, active low (strapping pin: */
                           /* do not hold it while resetting or it enters      */
                           /* download mode)                                   */

/* ------------------------------------------------------------------------- */
/* Timing                                                                     */
/* ------------------------------------------------------------------------- */
#define TELEMETRY_PERIOD_MS 200u /* 5 Hz (target, to validate)                 */
#define BRAKE_SAMPLE_MS      50u /* 20 Hz brake sampling (ASSUMPTION)          */
#define STATUS_LOG_MS      1000u /* 1 Hz serial status line                    */
#define MQTT_KEEPALIVE_S     10  /* LWT fires ~1.5x this after an unclean drop */
                                 /* (ASSUMPTION, MQTT 3.1.1; to validate)      */
#define RECONNECT_MIN_MS   1000u /* MQTT reconnect backoff: 1 s, doubling...   */
#define RECONNECT_MAX_MS  30000u /* ...capped at 30 s                          */
#define WIFI_RETRY_MS     15000u /* force a Wi-Fi reconnect if no IP this long */

/* ------------------------------------------------------------------------- */
/* Local logic. All values are bench defaults, not measured.                  */
/* ------------------------------------------------------------------------- */
/* Brake pot: 3.3 V -> 5.6 kohm -> 50 kohm pot -> 2.7 kohm -> GND, wiper to   */
/* PIN_BRAKE_POT, no capacitor (100 nF to GND optional, not fitted).          */
/* Calculated wiper range 0.15-2.98 V (not measured).                         */
#define BRAKE_MV_MIN        155  /* ASSUMPTION: wiper mV at 0 % (calibrate)    */
#define BRAKE_MV_MAX       2980  /* ASSUMPTION: wiper mV at 100 % (calibrate)  */
#define BRAKE_MV_FAULT_LO   80   /* ASSUMPTION: below -> wiper shorted/open    */
#define BRAKE_MV_FAULT_HI 3050   /* ASSUMPTION: above -> wiper open to 3V3     */
#define BRAKE_AVG_SAMPLES    8   /* ADC reads averaged per brake sample        */

#define BUTTON_DEBOUNCE_MS  30u  /* ASSUMPTION */

/* Local confirmation of a manual alert (node-local, not part of DEC-004). */
#define MANUAL_FEEDBACK_MS       300u /* ASSUMPTION: total confirmation time  */
#define MANUAL_FEEDBACK_BLINK_MS 100u /* ASSUMPTION: yellow on/off step       */
#define MANUAL_FEEDBACK_BEEP_MS  100u /* ASSUMPTION: beep at the start        */

/* Hard-brake thresholds (>= 70 %, >= 150 %/s, held 2 s) come from
 * alert_config_default() in firmware/common (ASSUMPTION, OPEN-005). */

#endif /* CONFIG_H */
