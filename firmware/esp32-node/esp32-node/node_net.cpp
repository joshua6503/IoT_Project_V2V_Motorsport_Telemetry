/**
 * node_net.cpp - see node_net.h.
 *
 * Topics and QoS follow interfaces/mqtt-topics.md:
 *   v2v/<node>/telemetry QoS0, v2v/<node>/alert QoS1,
 *   v2v/<node>/status QoS1 retained (online on connect, "offline" as LWT).
 */
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <mqtt_client.h>
#include <cJSON.h>

#include "config.h"
#include "node_net.h"

#define TOPIC_CMD       "v2v/" NODE_ID_STR "/cmd"
#define TOPIC_CMD_ALL   "v2v/all/cmd"
#define TOPIC_TELEMETRY "v2v/" NODE_ID_STR "/telemetry"
#define TOPIC_ALERT     "v2v/" NODE_ID_STR "/alert"
#define TOPIC_STATUS    "v2v/" NODE_ID_STR "/status"

#define STATUS_ONLINE  "{\"node_id\":\"" NODE_ID_STR "\",\"state\":\"online\"}"
#define STATUS_OFFLINE "{\"node_id\":\"" NODE_ID_STR "\",\"state\":\"offline\"}"

/* Outbox is only used by QoS 1 (alerts, online status); keep it small. */
#define OUTBOX_LIMIT_BYTES 4096

/* Telemetry (QoS 0) is sent by a small task with esp_mqtt_client_publish() so loop()
 * never waits on the socket. Bench 2026-11-23 findings:
 *  - esp_mqtt_client_enqueue(store=true) for QoS 0 delivers only ~1 msg/s to the broker.
 *  - esp_mqtt_client_publish() holds the client lock while it writes to the socket, so on a
 *    dead link (broker restart) it can stall the mqtt task: if the task makes no progress
 *    for TLM_STUCK_MS the node restarts itself (last resort, logged) instead of staying
 *    silently "connected" and mute.
 *  - Do not lower network.timeout_ms: on the home router an MQTT connect takes ~3 s and a
 *    3 s timeout made the node flap. Wi-Fi modem sleep is off for the same latency reason. */
#define TLM_QUEUE_DEPTH 4
#define TLM_STUCK_MS    20000u /* queue full and no task progress this long -> restart            */
#define TLM_NOPUB_MS    60000u /* Wi-Fi up but no telemetry accepted by the client this long -> restart */
typedef struct
{
    char json[160];
    int  len;
} tlm_item_t;
static QueueHandle_t     s_tlm_queue       = NULL;
static volatile uint32_t s_tlm_progress_ms = 0;
static volatile uint32_t s_last_pub_ok_ms  = 0; /* last esp_mqtt_client_publish() that returned >= 0 */

static esp_mqtt_client_handle_t s_client = NULL;

/* Remote commands (DEC-013). Parsed in the esp-mqtt task, consumed by loop() through a queue,
 * so the 5 Hz telemetry path and its watchdog are not involved. */
#define CMD_QUEUE_DEPTH 4
#define CMD_MAX_BYTES   255u
#define CMD_DEDUPE_N    8
#define CMD_DEFAULT_MS  3000u
#define CMD_MIN_MS      100u
#define CMD_MAX_MS      10000u
static QueueHandle_t s_cmd_queue = NULL;
typedef struct
{
    char     origin[NET_ID_MAX];
    uint32_t seq;
    bool     used;
} cmd_seen_t;
static cmd_seen_t s_seen[CMD_DEDUPE_N]; /* only touched from the esp-mqtt task */
static int        s_seen_next = 0;

/* Written by the Wi-Fi / MQTT tasks, read by loop(). */
static volatile bool     s_wifi_up       = false;
static volatile bool     s_mqtt_up       = false;
static volatile bool     s_retry_pending = false;
static volatile uint32_t s_next_retry_ms = 0;
static volatile uint32_t s_backoff_ms    = RECONNECT_MIN_MS;


/* Diagnostics, read by net_diag_str(). */
static volatile uint32_t s_pub_max_us   = 0; /* longest esp_mqtt_client_publish() since last read */
static volatile uint32_t s_pub_ok       = 0;
static volatile uint32_t s_pub_fail     = 0;
static volatile int      s_last_errno   = 0; /* last transport errno from MQTT_EVENT_ERROR */
static volatile int      s_last_errtype = 0;

static bool     s_started         = false;
static uint32_t s_attempt_ms      = 0;
static uint32_t s_wifi_down_since  = 0;

static bool time_reached(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

static void schedule_retry(void)
{
    uint32_t now     = millis();
    s_next_retry_ms  = now + s_backoff_ms;
    s_retry_pending  = true;
    Serial.printf("[mqtt] retry in %lu ms\n", (unsigned long)s_backoff_ms);
    s_backoff_ms = (s_backoff_ms * 2u > RECONNECT_MAX_MS) ? RECONNECT_MAX_MS : s_backoff_ms * 2u;
}

static void on_wifi_event(arduino_event_id_t event, arduino_event_info_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        s_wifi_up = true;
        Serial.printf("[wifi] connected, ip %s, bssid %s ch %d rssi %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.BSSIDstr().c_str(),
                      (int)WiFi.channel(), WiFi.RSSI());
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        if (s_wifi_up)
        {
            Serial.printf("[wifi] disconnected (reason %u)\n", (unsigned)info.wifi_sta_disconnected.reason);
        }
        s_wifi_up = false;
        break;
    default:
        break;
    }
}

/** node_id pattern ^[a-z0-9-]{1,32}$ (interfaces/payload.schema.json). */
static bool id_valid(const char *s)
{
    size_t n = (s != NULL) ? strlen(s) : 0;
    if (n < 1 || n > 32)
    {
        return false;
    }
    for (size_t i = 0; i < n; i++)
    {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
        {
            return false;
        }
    }
    return true;
}

/** Integer JSON number in [lo, hi]; false if missing, not a number, fractional or out of range. */
static bool json_uint(const cJSON *o, const char *key, double lo, double hi, uint32_t *out)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(it))
    {
        return false;
    }
    double v = it->valuedouble;
    if (v < lo || v > hi || v != (double)(uint64_t)v)
    {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static bool seen_before(const char *origin, uint32_t seq)
{
    for (int i = 0; i < CMD_DEDUPE_N; i++)
    {
        if (s_seen[i].used && s_seen[i].seq == seq && strcmp(s_seen[i].origin, origin) == 0)
        {
            return true;
        }
    }
    return false;
}

static void seen_add(const char *origin, uint32_t seq)
{
    strlcpy(s_seen[s_seen_next].origin, origin, NET_ID_MAX);
    s_seen[s_seen_next].seq  = seq;
    s_seen[s_seen_next].used = true;
    s_seen_next              = (s_seen_next + 1) % CMD_DEDUPE_N;
}

/** Validate one command payload and queue it. Logs exactly one [cmd] outcome line. */
static void handle_cmd(const char *topic, const char *payload)
{
    Serial.printf("[cmd] rx %s %s\n", topic, payload);
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL || !cJSON_IsObject(root))
    {
        Serial.println("[cmd] ignored: invalid JSON");
        cJSON_Delete(root);
        return;
    }

    net_cmd_t    c = {};
    const char  *reason = NULL;
    const cJSON *j;

    j = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    const char *cmd = cJSON_IsString(j) ? j->valuestring : NULL;
    if (cmd == NULL)
    {
        reason = "missing cmd";
    }
    else if (strcmp(cmd, "alert") == 0)
    {
        c.kind = NET_CMD_ALERT;
    }
    else if (strcmp(cmd, "clear") == 0)
    {
        c.kind = NET_CMD_CLEAR;
    }
    else
    {
        reason = "unknown cmd";
    }

    if (reason == NULL && !json_uint(root, "seq", 0, 4294967295.0, &c.seq))
    {
        reason = "bad seq";
    }
    if (reason == NULL)
    {
        j = cJSON_GetObjectItemCaseSensitive(root, "origin");
        if (!cJSON_IsString(j) || !id_valid(j->valuestring))
        {
            reason = "bad origin";
        }
        else
        {
            strlcpy(c.origin, j->valuestring, NET_ID_MAX);
        }
    }
    if (reason == NULL && c.kind == NET_CMD_ALERT)
    {
        j = cJSON_GetObjectItemCaseSensitive(root, "alert_type");
        const char *t = cJSON_IsString(j) ? j->valuestring : "";
        if (strcmp(t, "manual") == 0)          c.alert_type = V2V_ALERT_MANUAL;
        else if (strcmp(t, "hard_brake") == 0) c.alert_type = V2V_ALERT_HARD_BRAKE;
        else if (strcmp(t, "obstacle") == 0)   c.alert_type = V2V_ALERT_OBSTACLE;
        else if (strcmp(t, "overtemp") == 0)   c.alert_type = V2V_ALERT_OVERTEMP;
        else                                   reason = "bad alert_type";

        if (reason == NULL)
        {
            j = cJSON_GetObjectItemCaseSensitive(root, "severity");
            const char *s = cJSON_IsString(j) ? j->valuestring : "";
            if (strcmp(s, "info") == 0)          c.severity = V2V_SEVERITY_INFO;
            else if (strcmp(s, "warning") == 0)  c.severity = V2V_SEVERITY_WARNING;
            else if (strcmp(s, "critical") == 0) c.severity = V2V_SEVERITY_CRITICAL;
            else                                 reason = "bad severity";
        }
        if (reason == NULL)
        {
            c.duration_ms = CMD_DEFAULT_MS;
            if (cJSON_GetObjectItemCaseSensitive(root, "duration_ms") != NULL &&
                !json_uint(root, "duration_ms", CMD_MIN_MS, CMD_MAX_MS, &c.duration_ms))
            {
                reason = "bad duration_ms";
            }
        }
        if (reason == NULL)
        {
            j = cJSON_GetObjectItemCaseSensitive(root, "source_node");
            if (j != NULL)
            {
                /* node_id pattern, or "all" (also matches the pattern) */
                if (!cJSON_IsString(j) || !id_valid(j->valuestring))
                {
                    reason = "bad source_node";
                }
                else
                {
                    strlcpy(c.source_node, j->valuestring, NET_ID_MAX);
                }
            }
        }
    }

    if (reason == NULL && seen_before(c.origin, c.seq))
    {
        reason = "duplicate (origin, seq)";
    }
    if (reason == NULL && (s_cmd_queue == NULL || xQueueSend(s_cmd_queue, &c, 0) != pdTRUE))
    {
        reason = "command queue full";
    }

    if (reason != NULL)
    {
        Serial.printf("[cmd] ignored: %s\n", reason);
    }
    else
    {
        seen_add(c.origin, c.seq);
        Serial.printf("[cmd] queued %s origin=%s seq=%lu\n", (c.kind == NET_CMD_ALERT) ? "alert" : "clear",
                      c.origin, (unsigned long)c.seq);
    }
    cJSON_Delete(root);
}

static void on_mqtt_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        s_mqtt_up       = true;
        s_retry_pending = false;
        s_backoff_ms    = RECONNECT_MIN_MS;
        Serial.println("[mqtt] connected, publishing online");
        esp_mqtt_client_enqueue(s_client, TOPIC_STATUS, STATUS_ONLINE, 0, 1, 1, true);
        /* Subscriptions do not survive a clean-session reconnect: redo them every time. */
        Serial.printf("[cmd] subscribe %s msg_id=%d\n", TOPIC_CMD,
                      esp_mqtt_client_subscribe_single(s_client, TOPIC_CMD, 1));
        Serial.printf("[cmd] subscribe %s msg_id=%d\n", TOPIC_CMD_ALL,
                      esp_mqtt_client_subscribe_single(s_client, TOPIC_CMD_ALL, 1));
        break;
    case MQTT_EVENT_SUBSCRIBED:
        Serial.printf("[cmd] subscribed msg_id=%d\n", (int)ev->msg_id);
        break;
    case MQTT_EVENT_DATA:
    {
        char topic[40];
        char payload[CMD_MAX_BYTES + 1];
        int  tl = (ev->topic_len < (int)sizeof(topic) - 1) ? ev->topic_len : (int)sizeof(topic) - 1;
        if (ev->topic == NULL || ev->current_data_offset != 0 || ev->data_len != ev->total_data_len ||
            ev->data_len < 0 || ev->data_len > (int)CMD_MAX_BYTES)
        {
            Serial.printf("[cmd] ignored: fragmented or oversized message (%d bytes)\n", (int)ev->total_data_len);
            break;
        }
        memcpy(topic, ev->topic, tl);
        topic[tl] = 0;
        memcpy(payload, ev->data, ev->data_len);
        payload[ev->data_len] = 0;
        handle_cmd(topic, payload);
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        if (s_mqtt_up)
        {
            Serial.println("[mqtt] disconnected");
        }
        s_mqtt_up = false;
        if (!s_retry_pending)
        {
            schedule_retry();
        }
        break;
    case MQTT_EVENT_ERROR:
        if (ev != NULL && ev->error_handle != NULL)
        {
            s_last_errtype = (int)ev->error_handle->error_type;
            s_last_errno   = (int)ev->error_handle->esp_transport_sock_errno;
        }
        Serial.printf("[mqtt] error type=%d errno=%d\n", s_last_errtype, s_last_errno);
        break;
    default:
        break;
    }
}

static void telemetry_task(void *arg)
{
    (void)arg;
    tlm_item_t item;
    for (;;)
    {
        if (xQueueReceive(s_tlm_queue, &item, portMAX_DELAY) == pdTRUE)
        {
            if (s_mqtt_up)
            {
                uint32_t t0 = micros();
                int      r  = esp_mqtt_client_publish(s_client, TOPIC_TELEMETRY, item.json, item.len, 0, 0);
                uint32_t dt = micros() - t0;
                if (dt > s_pub_max_us)
                {
                    s_pub_max_us = dt;
                }
                if (r >= 0)
                {
                    s_last_pub_ok_ms = millis();
                    s_pub_ok = s_pub_ok + 1;
                }
                else
                {
                    s_pub_fail = s_pub_fail + 1;
                }
            }
            s_tlm_progress_ms = millis();
        }
    }
}

void net_init(void)
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false); /* modem sleep adds latency that breaks keepalive on a busy AP */
    WiFi.onEvent(on_wifi_event);

    /* Radio experiments (config.h, default off), see README "Wi-Fi stability findings". */
#if defined(WIFI_FORCE_HT20) && WIFI_FORCE_HT20
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
#endif
#if defined(WIFI_FORCE_11BG) && WIFI_FORCE_11BG
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
#endif

    /* Boot scan, diagnostics only: log every BSSID of our SSID (BSSID pinning was tested and
     * rejected, the node already joins the strongest one). */
    int n = WiFi.scanNetworks(false, false, false, 300);
    for (int i = 0; i < n; i++)
    {
        if (WiFi.SSID(i) == WIFI_SSID)
        {
            Serial.printf("[wifi] scan: \"%s\" bssid %s ch %d rssi %d dBm\n", WIFI_SSID,
                          WiFi.BSSIDstr(i).c_str(), (int)WiFi.channel(i), (int)WiFi.RSSI(i));
        }
    }
    WiFi.scanDelete();

    s_wifi_down_since = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#if defined(WIFI_TX_POWER_DBM_X4)
    WiFi.setTxPower((wifi_power_t)WIFI_TX_POWER_DBM_X4); /* units of 0.25 dBm, e.g. 34 = 8.5 dBm */
#endif
    Serial.printf("[wifi] connecting to \"%s\"\n", WIFI_SSID);

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.hostname  = MQTT_HOST;
    cfg.broker.address.port      = MQTT_PORT;
    cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    cfg.credentials.username     = MQTT_USER;
    cfg.credentials.client_id    = NODE_ID_STR;
    cfg.credentials.authentication.password = MQTT_PASSWORD;
    cfg.session.keepalive               = MQTT_KEEPALIVE_S;
    cfg.session.last_will.topic         = TOPIC_STATUS;
    cfg.session.last_will.msg           = STATUS_OFFLINE;
    cfg.session.last_will.msg_len       = 0; /* NUL-terminated */
    cfg.session.last_will.qos           = 1;
    cfg.session.last_will.retain        = 1;
    cfg.network.disable_auto_reconnect  = true; /* we do the backoff in net_loop() */
    cfg.outbox.limit                    = OUTBOX_LIMIT_BYTES;

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL)
    {
        Serial.println("[mqtt] client init FAILED");
        return;
    }
    esp_mqtt_client_register_event(s_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, on_mqtt_event, NULL);

    s_tlm_progress_ms = millis();
    s_last_pub_ok_ms  = millis();
    s_cmd_queue       = xQueueCreate(CMD_QUEUE_DEPTH, sizeof(net_cmd_t));
    s_tlm_queue       = xQueueCreate(TLM_QUEUE_DEPTH, sizeof(tlm_item_t));
    xTaskCreate(telemetry_task, "tlm_pub", 4096, NULL, 1, NULL);
}

static void restart_with_reason(const char *reason)
{
    Preferences prefs;
    prefs.begin("v2v", false);
    char buf[80];
    snprintf(buf, sizeof(buf), "boot%lu: %s", (unsigned long)prefs.getUInt("boots", 0), reason);
    prefs.putString("wd", buf);
    prefs.end();
    Serial.printf("[wd] restarting node: %s\n", reason);
    Serial.flush();
    ESP.restart();
}

void net_loop(uint32_t now_ms)
{
    /* Wi-Fi watchdog: the core auto-reconnects on most reasons; force it if no IP for too long. */
    if (s_wifi_up)
    {
        s_wifi_down_since = now_ms;
    }
    else if ((uint32_t)(now_ms - s_wifi_down_since) >= WIFI_RETRY_MS)
    {
        Serial.println("[wifi] no connection, forcing reconnect");
        WiFi.reconnect();
        s_wifi_down_since = now_ms;
    }

    /* Telemetry watchdog (last resort), independent of the MQTT state:
     *  (a) queue full and the publish task made no progress for TLM_STUCK_MS (blocked in publish()),
     *  (b) Wi-Fi is up but no telemetry was accepted by the client for TLM_NOPUB_MS.
     * The reason is logged and stored in NVS ("wd") before restarting. */
    if (s_tlm_queue != NULL)
    {
        if ((uxQueueSpacesAvailable(s_tlm_queue) == 0) && ((uint32_t)(now_ms - s_tlm_progress_ms) >= TLM_STUCK_MS))
        {
            restart_with_reason("tlm task stuck in publish (queue full)");
        }
        if (s_wifi_up && ((uint32_t)(now_ms - s_last_pub_ok_ms) >= TLM_NOPUB_MS))
        {
            restart_with_reason("no telemetry published for 60 s with wifi up");
        }
    }

    if (s_client == NULL || !s_wifi_up)
    {
        return;
    }

    if (!s_started)
    {
        if (esp_mqtt_client_start(s_client) == ESP_OK)
        {
            s_started    = true;
            s_attempt_ms = now_ms;
            Serial.printf("[mqtt] connecting to %s:%d\n", MQTT_HOST, (int)MQTT_PORT);
        }
        return;
    }

    if (s_mqtt_up)
    {
        s_attempt_ms = now_ms;
        return;
    }

    /* Safety net: no event for a long time while not connected -> schedule a retry. */
    if (!s_retry_pending && ((uint32_t)(now_ms - s_attempt_ms) >= 3u * RECONNECT_MAX_MS))
    {
        schedule_retry();
    }

    if (s_retry_pending && time_reached(now_ms, s_next_retry_ms))
    {
        s_retry_pending = false;
        s_attempt_ms    = now_ms;
        esp_err_t err   = esp_mqtt_client_reconnect(s_client);
        Serial.printf("[mqtt] reconnecting (%s)\n", (err == ESP_OK) ? "ok" : "client busy");
        if (err != ESP_OK)
        {
            schedule_retry();
        }
    }
}

bool net_cmd_poll(net_cmd_t *out)
{
    return (s_cmd_queue != NULL) && (xQueueReceive(s_cmd_queue, out, 0) == pdTRUE);
}

bool net_wifi_connected(void)
{
    return s_wifi_up;
}

bool net_mqtt_connected(void)
{
    return s_mqtt_up;
}

bool net_publish_telemetry(const char *json, int len)
{
    if (!s_mqtt_up || s_tlm_queue == NULL || len <= 0 || len >= (int)sizeof(((tlm_item_t *)0)->json))
    {
        return false;
    }
    tlm_item_t item;
    memcpy(item.json, json, len);
    item.json[len] = 0;
    item.len       = len;
    return xQueueSend(s_tlm_queue, &item, 0) == pdTRUE; /* full queue = link is slow: drop */
}

void net_diag_str(char *buf, size_t size)
{
    uint32_t max_us = s_pub_max_us;
    s_pub_max_us    = 0;
    snprintf(buf, size, "bssid=%s ch=%d rssi=%d pubmax=%lums pubok=%lu pubfail=%lu errno=%d/%d",
             WiFi.BSSIDstr().c_str(), (int)WiFi.channel(), (int)WiFi.RSSI(),
             (unsigned long)(max_us / 1000u), (unsigned long)s_pub_ok, (unsigned long)s_pub_fail,
             s_last_errtype, s_last_errno);
}

bool net_publish_alert(const char *json, int len)
{
    if (!s_mqtt_up)
    {
        return false;
    }
    return esp_mqtt_client_enqueue(s_client, TOPIC_ALERT, json, len, 1, 0, true) >= 0;
}
