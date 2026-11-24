/**
 * @file tlm_mqtt.c
 * @brief Wi-Fi join + MQTT client service. See tlm_mqtt.h.
 *
 * SDK references (paths relative to C:\nxp\SDK_RW612):
 *  - boards/frdmrw612/wifi_examples/wifi_mqtt/wifi_mqtt.c:68-96  WPL_AddNetwork -> WPL_Join -> IP
 *  - boards/frdmrw612/wifi_examples/wifi_mqtt/wifi_mqtt.c:112-124 WPL_Init, WPL_Start(link callback)
 *  - boards/frdmrw612/wifi_examples/wifi_mqtt/mqtt_freertos.c:178-219 connection callback + reconnect
 *    with sys_timeout, :221-232 mqtt_client_connect on the tcpip thread, :408-411 LOCK_TCPIP_CORE
 *  - middleware/lwip/src/include/lwip/apps/mqtt.h:68-87 mqtt_connect_client_info_t (user, pass, will_*)
 *  - components/edgefast_wifi/include/wpl.h WPL_* API (WPL_Join takes a non-const label)
 *
 *  - boards/frdmrw612/wifi_examples/wifi_mqtt/mqtt_freertos.c:96-176 subscribe + incoming publish
 *    callbacks (mqtt_set_inpub_callback, mqtt_subscribe from the connection callback)
 *  - middleware/lwip/src/include/lwip/apps/mqtt.h:136 MQTT_DATA_FLAG_LAST, :151 data cb, :163 publish cb,
 *    :188 mqtt_set_inpub_callback, :195 mqtt_subscribe
 *  - middleware/lwip/src/apps/mqtt/mqtt.c:1315-1322 mqtt_client_connect keeps the incoming callbacks
 *    (so they are set once), :762-782 publish cb once per message, data cb per chunk, PUBACK sent by lwIP
 *  - middleware/lwip/src/include/lwip/apps/mqtt_opts.h:64 rx buffer 128 B (payloads arrive in chunks),
 *    :71 four requests in flight
 *
 * Differences from the example: only the two DEC-013 command topics are subscribed (QoS 1, again on every
 * reconnect, retried until acknowledged), no DNS (IPv4 literal), the broker connection is retried forever,
 * the Wi-Fi join is retried instead of __BKPT, and credentials are never printed.
 */
#include "tlm_mqtt.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "wlan.h"
#include "wpl.h"

#include "board_io.h" /* board_log() only */
#include "tlm_json.h"

#define WIFI_LABEL             "v2v_ap"
#define WIFI_RETRY_MS          5000u
#define WIFI_REJOIN_AFTER_MS   15000u /* link down this long: re-join explicitly (ASSUMPTION: the driver may not) */
#define MQTT_RECONNECT_MS      1000u
#define MQTT_REFUSED_RETRY_MS  10000u
#define TOPIC_MAX              48u
#define PAYLOAD_MAX            48u
#define SUB_RETRY_MS           3000u
#define CMD_QUEUE_LEN          4u
#define TOPIC_ALL_CMD          "v2v/all/cmd"

static const tlm_mqtt_config_t *s_cfg;

static char s_topic_tlm[TOPIC_MAX];
static char s_topic_alert[TOPIC_MAX];
static char s_topic_status[TOPIC_MAX];
static char s_topic_cmd[TOPIC_MAX];
static char s_will_msg[PAYLOAD_MAX];
static char s_online_msg[PAYLOAD_MAX];
static int s_online_len;

static mqtt_client_t *s_client;
static struct mqtt_connect_client_info_t s_info;
static ip_addr_t s_broker;

static volatile bool s_link_up;
static volatile TickType_t s_link_down_tick;
static volatile bool s_connected;
static volatile bool s_ever_connected;

static volatile uint32_t s_wifi_drops;
static volatile uint32_t s_mqtt_drops;
static volatile uint32_t s_mqtt_reconnects;
static volatile uint32_t s_pub_ok;
static volatile uint32_t s_pub_err;
static volatile uint32_t s_cmd_rx;
static volatile uint32_t s_cmd_q_drop;

/* DEC-013 command intake. All of this runs on the lwIP tcpip thread; the tlm task only reads s_cmd_q. */
static QueueHandle_t s_cmd_q;
static char s_in_buf[ALERT_CMD_MAX_LEN];
static size_t s_in_len;
static bool s_in_overflow;
static int s_in_topic; /* 0 = not ours (ignore), 1 = v2v/{node}/cmd, 2 = v2v/all/cmd */
static bool s_sub_ok[2];
static volatile uint32_t s_sub_ok_count;

static char s_label[] = WIFI_LABEL; /* WPL_Join() takes a non-const char * */

/* ------------------------------------------------------------------------- */
/* Wi-Fi                                                                     */
/* ------------------------------------------------------------------------- */

static void link_status_cb(bool link_up)
{
    if (link_up)
    {
        s_link_up = true;
        board_log("wifi: link reestablished");
    }
    else
    {
        if (s_link_up)
        {
            s_wifi_drops++;
            s_link_down_tick = xTaskGetTickCount();
        }
        s_link_up = false;
        board_log("wifi: link lost");
    }
}

static void apply_ps_policy(void)
{
    enum wlan_ps_mode m = WLAN_ACTIVE;
    (void)wlan_get_ps_mode(&m);
    board_log("wifi: power save mode %d (0 active, 1 ieee, 2 deep sleep, 3 both)", (int)m);
    if ((s_cfg != NULL) && s_cfg->wifi_power_save_off)
    {
        int a = wlan_ieeeps_off();
        int b = wlan_deepsleepps_off();
        vTaskDelay(pdMS_TO_TICKS(200)); /* the requests are handled by the wlcmgr task */
        (void)wlan_get_ps_mode(&m);
        board_log("wifi: power save off requested (rc %d/%d), mode now %d", a, b, (int)m);
    }
}

static void log_ip(void)
{
    char ip[IP4ADDR_STRLEN_MAX];
    char gw[IP4ADDR_STRLEN_MAX];
    if (netif_default != NULL)
    {
        (void)ip4addr_ntoa_r(netif_ip4_addr(netif_default), ip, sizeof ip);
        (void)ip4addr_ntoa_r(netif_ip4_gw(netif_default), gw, sizeof gw);
        board_log("wifi: connected to \"%s\" ip=%s gw=%s", s_cfg->wifi_ssid, ip, gw);
    }
}

bool tlm_mqtt_wifi_join(const tlm_mqtt_config_t *cfg)
{
    if ((cfg == NULL) || (cfg->node_id == NULL) || (cfg->wifi_ssid == NULL) || (cfg->wifi_pass == NULL) ||
        (cfg->broker_ip == NULL))
    {
        board_log("wifi: invalid configuration");
        return false;
    }
    s_cfg = cfg;

    wpl_ret_t r = WPL_Init();
    if (r != WPLRET_SUCCESS)
    {
        board_log("wifi: WPL_Init failed %d", (int)r);
        return false;
    }
    r = WPL_Start(link_status_cb);
    if (r != WPLRET_SUCCESS)
    {
        board_log("wifi: WPL_Start failed %d", (int)r);
        return false;
    }
    r = WPL_AddNetwork(cfg->wifi_ssid, cfg->wifi_pass, s_label);
    if (r != WPLRET_SUCCESS)
    {
        board_log("wifi: WPL_AddNetwork failed %d", (int)r);
        return false;
    }

    board_log("wifi: joining \"%s\"...", cfg->wifi_ssid);
    for (;;)
    {
        r = WPL_Join(s_label);
        if (r == WPLRET_SUCCESS)
        {
            break;
        }
        /* 2 not found, 3 auth failed, 4 address failed, 7 timeout (wpl_ret_t order in wpl.h). */
        board_log("wifi: join failed %d, retry in %u s", (int)r, (unsigned)(WIFI_RETRY_MS / 1000u));
        vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
    }
    s_link_up = true;
    log_ip();
    apply_ps_policy();
    return true;
}

void tlm_mqtt_service(void)
{
    if (s_link_up || (s_cfg == NULL))
    {
        return;
    }
    if ((TickType_t)(xTaskGetTickCount() - s_link_down_tick) >= pdMS_TO_TICKS(WIFI_REJOIN_AFTER_MS))
    {
        board_log("wifi: link down for %u s, re-joining", (unsigned)(WIFI_REJOIN_AFTER_MS / 1000u));
        (void)WPL_Leave();
        wpl_ret_t r = WPL_Join(s_label);
        if (r == WPLRET_SUCCESS)
        {
            s_link_up = true;
            log_ip();
            apply_ps_policy();
        }
        else
        {
            board_log("wifi: re-join failed %d", (int)r);
            s_link_down_tick = xTaskGetTickCount(); /* wait another period before the next try */
        }
    }
}

bool tlm_mqtt_wifi_up(void)
{
    return s_link_up;
}

void tlm_mqtt_netif_text(char *buf, unsigned n)
{
    char ip[IP4ADDR_STRLEN_MAX] = "none";
    int up = 0, link = 0;
    if (netif_default != NULL)
    {
        (void)ip4addr_ntoa_r(netif_ip4_addr(netif_default), ip, sizeof ip);
        up   = netif_is_up(netif_default) ? 1 : 0;
        link = netif_is_link_up(netif_default) ? 1 : 0;
    }
    (void)snprintf(buf, n, "ip=%s up=%d link=%d", ip, up, link);
}

int tlm_mqtt_wifi_rssi(void)
{
    short rssi = 0;
    return (wlan_get_current_rssi(&rssi) == WM_SUCCESS) ? (int)rssi : 0;
}

/* ------------------------------------------------------------------------- */
/* MQTT (callbacks run on the lwIP tcpip thread)                              */
/* ------------------------------------------------------------------------- */

static void connect_to_mqtt(void *ctx);

static void schedule_connect(uint32_t delay_ms)
{
    sys_timeout(delay_ms, connect_to_mqtt, NULL);
}

/* ---- DEC-013 commands: incoming publishes (mqtt.c:762-782) ---- */

/** Once per received message, before its data (mqtt.h:163). The topic is only valid during this call. */
static void incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    (void)arg;
    s_in_len      = 0u;
    s_in_overflow = (tot_len > ALERT_CMD_MAX_LEN);
    if (strcmp(topic, s_topic_cmd) == 0)
    {
        s_in_topic = 1;
    }
    else if (strcmp(topic, TOPIC_ALL_CMD) == 0)
    {
        s_in_topic = 2;
    }
    else
    {
        s_in_topic = 0;
    }
}

/** Once per chunk; the last one carries MQTT_DATA_FLAG_LAST (mqtt.h:136,151). */
static void incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    (void)arg;
    if (s_in_topic == 0)
    {
        return;
    }
    if (!s_in_overflow)
    {
        if ((s_in_len + len) <= sizeof s_in_buf)
        {
            memcpy(&s_in_buf[s_in_len], data, len);
            s_in_len += len;
        }
        else
        {
            s_in_overflow = true;
        }
    }
    if ((flags & MQTT_DATA_FLAG_LAST) == 0u)
    {
        return;
    }

    tlm_mqtt_cmd_t msg;
    msg.broadcast = (s_in_topic == 2);
    msg.status    = s_in_overflow ? ALERT_CMD_ERR_TOO_LONG : alert_cmd_parse(s_in_buf, s_in_len, &msg.cmd);
    if (s_in_overflow)
    {
        memset(&msg.cmd, 0, sizeof msg.cmd);
    }
    s_cmd_rx++;
    s_in_topic = 0;
    if ((s_cmd_q == NULL) || (xQueueSend(s_cmd_q, &msg, 0) != pdTRUE))
    {
        s_cmd_q_drop++;
    }
}

/* ---- DEC-013 commands: subscribe on every (re)connect (mqtt.h:195, mqtt_freertos.c:150-170) ---- */

static void subscribe_cb(void *arg, err_t err)
{
    int idx = (int)(intptr_t)arg;
    if (err == ERR_OK)
    {
        if (!s_sub_ok[idx])
        {
            s_sub_ok[idx] = true;
            s_sub_ok_count++;
            board_log("mqtt: subscribed %s (QoS 1)", (idx == 0) ? s_topic_cmd : TOPIC_ALL_CMD);
        }
    }
    else
    {
        board_log("mqtt: subscribe %s failed %d", (idx == 0) ? s_topic_cmd : TOPIC_ALL_CMD, (int)err);
    }
}

static void subscribe_cmd_topics(void *ctx)
{
    (void)ctx;
    sys_untimeout(subscribe_cmd_topics, NULL);
    if (!s_connected || (s_client == NULL))
    {
        return; /* the next ACCEPTED connection starts over */
    }
    for (int i = 0; i < 2; i++)
    {
        if (!s_sub_ok[i])
        {
            err_t err = mqtt_subscribe(s_client, (i == 0) ? s_topic_cmd : TOPIC_ALL_CMD, 1, subscribe_cb,
                                       (void *)(intptr_t)i);
            if (err != ERR_OK)
            {
                board_log("mqtt: subscribe call %d failed %d", i, (int)err);
            }
        }
    }
    if (!(s_sub_ok[0] && s_sub_ok[1]))
    {
        sys_timeout(SUB_RETRY_MS, subscribe_cmd_topics, NULL); /* until both are acknowledged */
    }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    (void)arg;
    bool was_connected = s_connected;
    s_connected        = (status == MQTT_CONNECT_ACCEPTED);

    switch (status)
    {
        case MQTT_CONNECT_ACCEPTED:
            if (s_ever_connected)
            {
                s_mqtt_reconnects++;
            }
            s_ever_connected = true;
            board_log("mqtt: connected to %s:%u as \"%s\" (reconnects=%u)", ipaddr_ntoa(&s_broker),
                      (unsigned)s_cfg->broker_port, s_cfg->node_id, (unsigned)s_mqtt_reconnects);
            /* Retained "online"; the broker publishes the retained "offline" LWT if we vanish. */
            if (mqtt_publish(client, s_topic_status, s_online_msg, (u16_t)s_online_len, 1, 1, NULL, NULL) != ERR_OK)
            {
                s_pub_err++;
                board_log("%s", "mqtt: online status publish failed");
            }
            /* The broker forgot our subscriptions (clean session): subscribe again on every connect. */
            s_sub_ok[0]    = false;
            s_sub_ok[1]    = false;
            s_sub_ok_count = 0u;
            subscribe_cmd_topics(NULL);
            break;

        case MQTT_CONNECT_DISCONNECTED:
        case MQTT_CONNECT_TIMEOUT:
            s_sub_ok[0]    = false;
            s_sub_ok[1]    = false;
            s_sub_ok_count = 0u;
            if (was_connected)
            {
                s_mqtt_drops++;
            }
            board_log("mqtt: %s (status %d), retry in %u ms",
                      (status == MQTT_CONNECT_DISCONNECTED) ? "disconnected" : "connect timeout", (int)status,
                      (unsigned)MQTT_RECONNECT_MS);
            schedule_connect(MQTT_RECONNECT_MS);
            break;

        default:
            /* Refused: wrong credentials or ACL; do not hammer the broker. */
            board_log("mqtt: connection refused (status %d), retry in %u s", (int)status,
                      (unsigned)(MQTT_REFUSED_RETRY_MS / 1000u));
            schedule_connect(MQTT_REFUSED_RETRY_MS);
            break;
    }
}

static void connect_to_mqtt(void *ctx)
{
    (void)ctx;

    if (!s_link_up)
    {
        schedule_connect(MQTT_RECONNECT_MS); /* no point in connecting without a link */
        return;
    }
    err_t err = mqtt_client_connect(s_client, &s_broker, s_cfg->broker_port, mqtt_connection_cb, NULL, &s_info);
    if (err != ERR_OK)
    {
        char nif[48];
        tlm_mqtt_netif_text(nif, sizeof nif);
        board_log("mqtt: connect call failed %d (%s), retry in %u ms", (int)err, nif, (unsigned)MQTT_RECONNECT_MS);
        schedule_connect(MQTT_RECONNECT_MS);
    }
}

static void start_on_tcpip_thread(void *ctx)
{
    connect_to_mqtt(ctx);
}

bool tlm_mqtt_start(void)
{
    if (s_cfg == NULL)
    {
        return false;
    }
    if (!ipaddr_aton(s_cfg->broker_ip, &s_broker) || !IP_IS_V4(&s_broker))
    {
        board_log("mqtt: broker_ip \"%s\" is not an IPv4 literal", s_cfg->broker_ip);
        return false;
    }

    int a = snprintf(s_topic_tlm, sizeof s_topic_tlm, "v2v/%s/telemetry", s_cfg->node_id);
    int b = snprintf(s_topic_alert, sizeof s_topic_alert, "v2v/%s/alert", s_cfg->node_id);
    int c = snprintf(s_topic_status, sizeof s_topic_status, "v2v/%s/status", s_cfg->node_id);
    int d = snprintf(s_topic_cmd, sizeof s_topic_cmd, "v2v/%s/cmd", s_cfg->node_id);
    s_online_len = tlm_json_status(s_online_msg, sizeof s_online_msg, s_cfg->node_id, true);
    int w        = tlm_json_status(s_will_msg, sizeof s_will_msg, s_cfg->node_id, false);
    if ((a <= 0) || (b <= 0) || (c <= 0) || (d <= 0) || ((size_t)a >= sizeof s_topic_tlm) ||
        ((size_t)b >= sizeof s_topic_alert) || ((size_t)c >= sizeof s_topic_status) ||
        ((size_t)d >= sizeof s_topic_cmd) || (s_online_len < 0) || (w < 0))
    {
        board_log("%s", "mqtt: invalid node_id");
        return false;
    }

    memset(&s_info, 0, sizeof s_info);
    s_info.client_id   = s_cfg->node_id;
    s_info.client_user = ((s_cfg->mqtt_user != NULL) && (s_cfg->mqtt_user[0] != '\0')) ? s_cfg->mqtt_user : NULL;
    s_info.client_pass = ((s_cfg->mqtt_pass != NULL) && (s_cfg->mqtt_pass[0] != '\0')) ? s_cfg->mqtt_pass : NULL;
    s_info.keep_alive  = s_cfg->keepalive_s;
    s_info.will_topic  = s_topic_status;
    s_info.will_msg    = s_will_msg;
    s_info.will_qos    = 1;
    s_info.will_retain = 1;

    s_cmd_q = xQueueCreate(CMD_QUEUE_LEN, sizeof(tlm_mqtt_cmd_t));
    if (s_cmd_q == NULL)
    {
        board_log("%s", "mqtt: command queue creation failed (heap?)");
        return false;
    }

    LOCK_TCPIP_CORE();
    s_client = mqtt_client_new();
    if (s_client != NULL)
    {
        /* Set once: mqtt_client_connect() keeps the incoming callbacks across reconnects (mqtt.c:1315-1322). */
        mqtt_set_inpub_callback(s_client, incoming_publish_cb, incoming_data_cb, NULL);
    }
    UNLOCK_TCPIP_CORE();
    if (s_client == NULL)
    {
        board_log("%s", "mqtt: mqtt_client_new failed");
        return false;
    }

    board_log("mqtt: connecting to %s:%u (user %s, keepalive %u s)", ipaddr_ntoa(&s_broker), (unsigned)s_cfg->broker_port,
              (s_info.client_user != NULL) ? "set" : "none", (unsigned)s_info.keep_alive);
    if (tcpip_callback(start_on_tcpip_thread, NULL) != ERR_OK)
    {
        board_log("%s", "mqtt: tcpip_callback failed");
        return false;
    }
    return true;
}

bool tlm_mqtt_connected(void)
{
    return s_connected;
}

static bool publish(const char *topic, const char *json, uint16_t len, u8_t qos)
{
    if (!s_connected || (s_client == NULL))
    {
        s_pub_err++;
        return false;
    }
    LOCK_TCPIP_CORE();
    err_t err = mqtt_publish(s_client, topic, json, len, qos, 0, NULL, NULL);
    UNLOCK_TCPIP_CORE();
    if (err != ERR_OK)
    {
        s_pub_err++;
        return false;
    }
    s_pub_ok++;
    return true;
}

bool tlm_mqtt_publish_telemetry(const char *json, uint16_t len)
{
    return publish(s_topic_tlm, json, len, 0);
}

bool tlm_mqtt_publish_alert(const char *json, uint16_t len)
{
    return publish(s_topic_alert, json, len, 1);
}

void tlm_mqtt_get_stats(tlm_mqtt_stats_t *out)
{
    if (out == NULL)
    {
        return;
    }
    out->wifi_drops      = s_wifi_drops;
    out->mqtt_drops      = s_mqtt_drops;
    out->mqtt_reconnects = s_mqtt_reconnects;
    out->pub_ok          = s_pub_ok;
    out->pub_err         = s_pub_err;
    out->cmd_rx          = s_cmd_rx;
    out->cmd_q_drop      = s_cmd_q_drop;
    out->sub_ok          = s_sub_ok_count;
}

bool tlm_mqtt_cmd_poll(tlm_mqtt_cmd_t *out)
{
    return (out != NULL) && (s_cmd_q != NULL) && (xQueueReceive(s_cmd_q, out, 0) == pdTRUE);
}
