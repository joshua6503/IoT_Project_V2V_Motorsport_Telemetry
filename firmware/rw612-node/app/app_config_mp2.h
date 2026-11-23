/**
 * @file app_config_mp2.h
 * @brief MP2 (Wi-Fi + MQTT, FreeRTOS) build-time configuration, on top of app_config.h.
 *
 * app_config.h (periods, FIR switch, pot calibration) is shared with MP1 and is NOT changed.
 * Credentials do NOT go here: see net_config.example.h.
 */
#ifndef APP_CONFIG_MP2_H
#define APP_CONFIG_MP2_H

/* ------------------------------------------------------------------------- */
/* Node identity and MQTT                                                    */
/* ------------------------------------------------------------------------- */
#define APP_NODE_ID          "rw612-a" /* interfaces/mqtt-topics.md, ^[a-z0-9-]{1,32}$ */
#define APP_MQTT_KEEPALIVE_S 10u       /* ASSUMPTION: broker declares the node dead after 1.5x = 15 s */

/** Telemetry publish period: 5 Hz target (to validate, interfaces/mqtt-topics.md). */
#define APP_TLM_PERIOD_MS 200u
/** The tlm task wakes this often; alerts are drained every wake-up, telemetry every APP_TLM_PERIOD_MS. */
#define APP_TLM_TICK_MS 50u

/* ------------------------------------------------------------------------- */
/* FreeRTOS tasks. configMAX_PRIORITIES = 5 in the wifi_mqtt base            */
/* (FreeRTOSConfig.h:45). Wi-Fi driver/core/wlcmgr tasks run at 3, tcpip at  */
/* 2, timer service at 4 (see README "Priorities").                          */
/* ------------------------------------------------------------------------- */
/** Sensor task priority. 4 = above the Wi-Fi tasks (first try). Fallback B: 2 (see README). */
#define APP_SENSE_PRIO        4u
#define APP_SENSE_STACK_WORDS 1024u /* ASSUMPTION, check free heap in the log */
#define APP_SENSE_YIELD_MS    1u    /* the sensor task blocks this long between iterations */

/** MQTT/telemetry task priority = lwIP DEFAULT_THREAD_PRIO (lwipopts.h:103). */
#define APP_TLM_PRIO        1u
#define APP_TLM_STACK_WORDS 1536u /* ASSUMPTION, check free heap in the log */

#define APP_ALERT_QUEUE_LEN 8u

/**
 * 1: wrap only the DHT11 40-bit frame read (about 4-5 ms) in taskENTER_CRITICAL, masking
 * interrupts at or below configMAX_SYSCALL_INTERRUPT_PRIORITY (FreeRTOSConfigBoard.h:74,81).
 * Default 0: enable only if the DHT11 checksum/timeout counters in the log are high.
 */
#define APP_DHT11_FRAME_CRITICAL 0

/** 1: turn the Wi-Fi driver's IEEE and deep-sleep power save off after each join (the SDK enables it by default,
 *  CONFIG_WIFI_AUTO_POWER_SAVE 1 in wifi_config_default.h:56-58; a sleeping client missed ARP/frames on the hotspot). */
#define APP_WIFI_POWER_SAVE_OFF 0

/** Diagnostic only: 1 = do not create the sense task (Wi-Fi/MQTT link measured without the sensor load; no telemetry is published). */
#define APP_DIAG_NO_SENSE 0

#endif /* APP_CONFIG_MP2_H */
