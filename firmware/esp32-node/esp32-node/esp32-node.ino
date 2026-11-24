/**
 * esp32-node.ino - secondary V2V node B (ESP32 classic, Arduino core).
 *
 * Phase F2: Wi-Fi + MQTT only (no BLE yet, see interfaces/ and ARQUITECTURA §8).
 *  - brake pot  -> brake % -> alert_evaluate() (firmware/common) -> hard_brake alert
 *  - BOOT button -> manual alert (info) + node-local confirmation
 *  - LEDs / buzzer driven by alert_pattern_for() (DEC-004)
 *  - telemetry 5 Hz, alerts QoS 1, status retained + LWT (node_net.cpp)
 * The main loop never blocks: no delay(), everything is scheduled with millis().
 */
#include <Preferences.h>
#include <esp_system.h>

#include "config.h"
#include "node_io.h"
#include "node_net.h"
#include "src/alert_logic.h"
#include "src/alert_pattern.h"
#include "src/v2v_protocol.h"

static alert_ctx_t s_alert_ctx;
static alert_t     s_alert = {V2V_ALERT_NONE, V2V_SEVERITY_INFO, false};

static float    s_brake_pct  = 0.0f;
static uint32_t s_brake_mv   = 0;
static uint8_t  s_sensor_ok  = 0;
static uint32_t s_tlm_seq    = 0;
static uint16_t s_alert_seq  = 0;
static uint32_t s_tlm_sent   = 0;
static uint32_t s_tlm_drop   = 0;

/* Boot diagnostics: reboot counter and reset reason, persisted in NVS, serial only. */
static uint32_t    s_boot_count   = 0;
static const char *s_reset_reason = "?";
static char        s_prev_rst[16]  = "?"; /* reset reason of the previous boot (NVS) */
static char        s_wd_reason[80] = "-"; /* last watchdog restart reason (NVS), "-" if none */

static const char *reset_reason_str(esp_reset_reason_t r)
{
    switch (r)
    {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
    }
}

static uint32_t s_next_brake_ms  = 0;
static uint32_t s_next_tlm_ms    = 0;
static uint32_t s_next_status_ms = 0;

static const char *alert_type_str(v2v_alert_type_t t)
{
    switch (t)
    {
    case V2V_ALERT_MANUAL:     return "manual";
    case V2V_ALERT_HARD_BRAKE: return "hard_brake";
    case V2V_ALERT_OBSTACLE:   return "obstacle";
    case V2V_ALERT_OVERTEMP:   return "overtemp";
    default:                   return "none";
    }
}

static const char *severity_str(v2v_severity_t s)
{
    switch (s)
    {
    case V2V_SEVERITY_WARNING:  return "warning";
    case V2V_SEVERITY_CRITICAL: return "critical";
    default:                    return "info";
    }
}

/** true when @p *next is due; advances it by @p period without accumulating drift. */
static bool due(uint32_t now, uint32_t *next, uint32_t period)
{
    if ((int32_t)(now - *next) < 0)
    {
        return false;
    }
    *next += period;
    if ((int32_t)(now - *next) >= 0) /* fell far behind: resync instead of bursting */
    {
        *next = now + period;
    }
    return true;
}

static void raise_alert(v2v_alert_type_t type, v2v_severity_t sev, uint32_t now)
{
    char json[160];
    int  n = snprintf(json, sizeof(json),
                     "{\"node_id\":\"%s\",\"ts_ms\":%lu,\"seq\":%u,\"alert_type\":\"%s\",\"severity\":\"%s\"}",
                     NODE_ID_STR, (unsigned long)now, (unsigned)s_alert_seq,
                     alert_type_str(type), severity_str(sev));
    bool sent = net_publish_alert(json, n);
    Serial.printf("[alert] %s/%s seq=%u %s\n", alert_type_str(type), severity_str(sev),
                  (unsigned)s_alert_seq, sent ? "published" : "NOT published (mqtt down)");
    s_alert_seq++; /* uint16, wraps like v2v_packet_t.seq */
}

static void sample_brake(uint32_t now)
{
    s_brake_mv = io_read_brake_mv();
    bool valid = io_brake_mv_valid(s_brake_mv);
    s_brake_pct = valid ? io_brake_pct_from_mv(s_brake_mv) : 0.0f;
    s_sensor_ok = valid ? ALERT_SENSOR_BRAKE : 0u;

    /* Node B has no distance/temperature sensor: those bits stay clear, so no
     * OBSTACLE/OVERTEMP can fire; only the brake sensor decides sensor_fault. */
    telemetry_t t = {};
    t.ts_ms       = now;
    t.distance_cm = -1.0f;
    t.brake_pct   = s_brake_pct;
    t.sensor_ok   = s_sensor_ok;

    v2v_alert_type_t prev = s_alert.type;
    alert_evaluate(&s_alert_ctx, &t, &s_alert);
    s_alert.sensor_fault = (s_sensor_ok & ALERT_SENSOR_BRAKE) == 0u;

    if ((s_alert.type == V2V_ALERT_HARD_BRAKE) && (prev != V2V_ALERT_HARD_BRAKE))
    {
        raise_alert(V2V_ALERT_HARD_BRAKE, s_alert.severity, now);
    }
}

static void publish_telemetry(uint32_t now)
{
    char json[160];
    int  n = snprintf(json, sizeof(json),
                     "{\"node_id\":\"%s\",\"ts_ms\":%lu,\"seq\":%lu,\"brake_pct\":%.1f,\"sensor_ok\":%u}",
                     NODE_ID_STR, (unsigned long)now, (unsigned long)s_tlm_seq,
                     (double)s_brake_pct, (unsigned)s_sensor_ok);
    s_tlm_seq++; /* counts every sample, so a gap on the broker means loss */
    if (net_publish_telemetry(json, n))
    {
        s_tlm_sent++;
    }
    else
    {
        s_tlm_drop++;
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.printf("\n[%s] v2v protocol v%u.%u, F2 wifi+mqtt\n", NODE_ID_STR,
                  (unsigned)V2V_PROTOCOL_VERSION_MAJOR, (unsigned)V2V_PROTOCOL_VERSION_MINOR);

    /* Boot diagnostics, serial only (the MQTT payload contract is frozen). */
    Preferences prefs;
    prefs.begin("v2v", false);
    s_boot_count = prefs.getUInt("boots", 0) + 1;
    prefs.putUInt("boots", s_boot_count);
    prefs.getString("rst", "?").toCharArray(s_prev_rst, sizeof(s_prev_rst));
    prefs.getString("wd", "-").toCharArray(s_wd_reason, sizeof(s_wd_reason));
    s_reset_reason = reset_reason_str(esp_reset_reason());
    prefs.putString("rst", s_reset_reason);
    prefs.end();
    Serial.printf("[boot] count=%lu reset_reason=%s prev_rst=%s wd=\"%s\"\n", (unsigned long)s_boot_count,
                  s_reset_reason, s_prev_rst, s_wd_reason);

    io_init();
#if defined(BUZZER_SELFTEST) && BUZZER_SELFTEST
    io_buzzer_selftest();
#endif
    alert_init(&s_alert_ctx, NULL); /* bench defaults from firmware/common (ASSUMPTION, OPEN-005) */
    net_init();

    uint32_t now     = millis();
    s_next_brake_ms  = now;
    s_next_tlm_ms    = now + TELEMETRY_PERIOD_MS;
    s_next_status_ms = now + STATUS_LOG_MS;
}

void loop()
{
    uint32_t now = millis();

    net_loop(now);

    if (due(now, &s_next_brake_ms, BRAKE_SAMPLE_MS))
    {
        sample_brake(now);
    }

    if (io_button_pressed(now))
    {
        Serial.println("[btn] BOOT pressed");
        io_manual_feedback(now);
        raise_alert(V2V_ALERT_MANUAL, V2V_SEVERITY_INFO, now);
    }

    if (due(now, &s_next_tlm_ms, TELEMETRY_PERIOD_MS))
    {
        publish_telemetry(now);
    }

    /* Remote commands (DEC-013): actuate only, never publish an alert (no loops). */
    net_cmd_t cmd;
    while (net_cmd_poll(&cmd))
    {
        if (cmd.kind == NET_CMD_ALERT)
        {
            alert_t ra = {cmd.alert_type, cmd.severity, false};
            io_remote_alert(&ra, cmd.duration_ms, now);
            Serial.printf("[cmd] applied alert %s/%s %lu ms origin=%s seq=%lu source=%s\n",
                          alert_type_str(cmd.alert_type), severity_str(cmd.severity),
                          (unsigned long)cmd.duration_ms, cmd.origin, (unsigned long)cmd.seq,
                          (cmd.source_node[0] != 0) ? cmd.source_node : "-");
        }
        else
        {
            io_remote_clear(now);
            Serial.printf("[cmd] applied clear origin=%s seq=%lu\n", cmd.origin, (unsigned long)cmd.seq);
        }
    }

    alert_t         shown = io_effective_alert(&s_alert, now);
    alert_pattern_t p     = alert_pattern_for(&shown, now);
    io_apply(&p, now);

    if (due(now, &s_next_status_ms, STATUS_LOG_MS))
    {
        char diag[128];
        net_diag_str(diag, sizeof(diag));
        Serial.printf("[st] wifi=%d mqtt=%d brake=%.1f%% (%lu mV) sensor_ok=%u alert=%s remote=%d tlm sent=%lu drop=%lu boots=%lu rst=%s prev=%s wd=\"%s\" %s\n",
                      (int)net_wifi_connected(), (int)net_mqtt_connected(), (double)s_brake_pct,
                      (unsigned long)s_brake_mv, (unsigned)s_sensor_ok, alert_type_str(s_alert.type),
                      (int)io_remote_active(now), (unsigned long)s_tlm_sent, (unsigned long)s_tlm_drop, (unsigned long)s_boot_count,
                      s_reset_reason, s_prev_rst, s_wd_reason, diag);
    }
}
