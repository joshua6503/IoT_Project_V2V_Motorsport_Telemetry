/**
 * @file main_mp2.c
 * @brief MP2 / phase F3: the MP1 node (sense -> decide -> actuate) plus Wi-Fi/MQTT telemetry.
 *
 * Replaces main_mp1.c in the wifi_mqtt-based project (exclude main_mp1.c from that build).
 * FreeRTOS + lwIP (DEC-012 proposal; DEC-001 superloop retired). Two tasks:
 *
 *  - "sense" (APP_SENSE_PRIO, above the Wi-Fi tasks): the MP1 loop logic unchanged. DHT11 every
 *    APP_DHT11_PERIOD_MS, pot slot every APP_SENSOR_PERIOD_MS on a fixed grid resynchronised when
 *    more than one period late (DEC-010), HC-SR04 every APP_HCSR04_PERIOD_MS (blocks up to ~35 ms),
 *    alert_evaluate() and the local LEDs/buzzer. It never prints and never waits for the network:
 *    the local safety path does not depend on Wi-Fi or MQTT (ARQUITECTURA principle 1). It blocks
 *    APP_SENSE_YIELD_MS between iterations so lower-priority tasks (Wi-Fi, lwIP, tlm) run.
 *  - "tlm" (APP_TLM_PRIO): joins Wi-Fi, connects MQTT, publishes the latest snapshot every
 *    APP_TLM_PERIOD_MS (QoS 0), mirrors every new alert (QoS 1), prints the log line.
 *
 * Data between them: a length-1 queue (latest snapshot, overwrite/peek) and a small alert queue.
 * board_time_*() and board_delay_us() are called from the sense task ONLY (their wrap extension in
 * board_io.c is not reentrant); the tlm task uses RTOS ticks.
 *
 * "New alert" (ASSUMPTION): alert_evaluate() returns an alert whose (type, severity) differs from the
 * previous sample's and is not NONE. A latched HARD_BRAKE is therefore published once, not every 50 ms.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "alert_actuation.h"
#include "alert_logic.h"
#include "app_config.h"
#include "app_config_mp2.h"
#include "board_io.h"
#include "dht11.h"
#include "hcsr04.h"
#include "leds.h"
#include "pot.h"
#include "tlm_json.h"
#include "tlm_mqtt.h"

#if defined(__has_include)
#if !__has_include("net_config.h")
#error "app/net_config.h is missing: copy app/net_config.example.h to app/net_config.h and fill it in (git-ignored)"
#endif
#endif
#include "net_config.h"

#if APP_ENABLE_BRAKE_FIR
#include "fir_brake_coeffs.h"
#include "fir_filter.h"
#endif

#if APP_LOG_DSP_CSV
#error "APP_LOG_DSP_CSV is an MP1/PDS capture mode; the sense task does not print (set it to 0)"
#endif

/* Sensor timing limits from the datasheets, enforced at build time (DEC-006). */
#if APP_HCSR04_PERIOD_MS < HCSR04_MIN_CYCLE_MS
#error "APP_HCSR04_PERIOD_MS below the HC-SR04 measurement cycle (HCSR04_MIN_CYCLE_MS)"
#endif
#if APP_DHT11_PERIOD_MS < DHT11_MIN_INTERVAL_MS
#error "APP_DHT11_PERIOD_MS below the DHT11 sampling period (DHT11_MIN_INTERVAL_MS)"
#endif
#if (APP_TLM_PERIOD_MS % APP_TLM_TICK_MS) != 0u
#error "APP_TLM_PERIOD_MS must be a multiple of APP_TLM_TICK_MS"
#endif
#if (APP_SENSOR_TOPIC_PERIOD_MS % APP_TLM_TICK_MS) != 0u
#error "APP_SENSOR_TOPIC_PERIOD_MS must be a multiple of APP_TLM_TICK_MS"
#endif
/* Sensor topics go out on ticks = 1 (mod per_sens), telemetry on ticks = 0 (mod per_tlm). Both periods even in
 * ticks -> odd vs even ticks, so the two never share a wake-up. */
#if (((APP_SENSOR_TOPIC_PERIOD_MS / APP_TLM_TICK_MS) % 2u) != 0u) || (((APP_TLM_PERIOD_MS / APP_TLM_TICK_MS) % 2u) != 0u)
#error "sensor-topic and telemetry periods must both be an even number of APP_TLM_TICK_MS"
#endif
#if APP_SENSE_PRIO >= configMAX_PRIORITIES
#error "APP_SENSE_PRIO must be below configMAX_PRIORITIES"
#endif

/* ------------------------------------------------------------------------- */
/* Data shared between the tasks                                             */
/* ------------------------------------------------------------------------- */

typedef struct
{
    uint32_t ts_ms; /* board_time_ms() at the sample */
    float brake_pct;
    float temp_c;
    float distance_cm;
    float tps_pct;
    bool temp_valid;
    bool dist_valid;
    bool has_tps;
    uint8_t sensor_ok;
    v2v_alert_type_t alert_type;
    v2v_severity_t alert_sev;
    bool sensor_fault;
    /* diagnostics for the log line */
    float brake_raw_pct;
    float brake_filtered_pct;
    int32_t brake_raw;
    int dht_st;
    uint32_t dht_ok;
    uint32_t dht_cks;
    uint32_t dht_to;
    uint32_t dht_nr;
    unsigned echo_us;
    int echo_err;
    uint32_t slot_late_max_ms; /* worst lateness of the 50 ms pot slot (DEC-010 monitoring) */
    uint32_t slot_resyncs;     /* times the grid was resynchronised (more than one period late) */
    bool rem_active;           /* a dashboard command (DEC-013) alert is pending */
    v2v_severity_t rem_sev;
    int32_t adc_int[BOARD_ADC_INTERNAL_COUNT]; /* APP_ADC_DIAG: vref_12, vbat_s, vssa raw (INT32_MIN = none) */
} snapshot_t;

typedef struct
{
    uint32_t ts_ms;
    uint16_t seq;
    v2v_alert_type_t type;
    v2v_severity_t severity;
} alert_event_t;

static QueueHandle_t s_snap_q;
static QueueHandle_t s_alert_q;
static QueueHandle_t s_remote_q; /* tlm task -> sense task: accepted dashboard commands (DEC-013) */
static volatile uint32_t s_alert_drops; /* alert events lost because the queue was full */

/* Command counters, written by the tlm task only. */
static alert_cmd_dedupe_t s_dedupe;
static uint32_t s_cmd_ok;
static uint32_t s_cmd_bad;
static uint32_t s_cmd_dup;
static uint32_t s_remote_drops;

static const tlm_mqtt_config_t s_net_cfg = {
    .node_id     = APP_NODE_ID,
    .wifi_ssid   = NET_WIFI_SSID,
    .wifi_pass   = NET_WIFI_PASSPHRASE,
    .broker_ip   = NET_MQTT_BROKER_IP,
    .broker_port = NET_MQTT_PORT,
    .mqtt_user   = NET_MQTT_USER,
    .mqtt_pass   = NET_MQTT_PASS,
    .keepalive_s = APP_MQTT_KEEPALIVE_S,
    .wifi_power_save_off = (APP_WIFI_POWER_SAVE_OFF != 0),
};

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static const char *alert_name(v2v_alert_type_t t)
{
    switch (t)
    {
        case V2V_ALERT_MANUAL:
            return "MANUAL";
        case V2V_ALERT_HARD_BRAKE:
            return "HARD_BRAKE";
        case V2V_ALERT_OBSTACLE:
            return "OBSTACLE";
        case V2V_ALERT_OVERTEMP:
            return "OVERTEMP";
        default:
            return "NONE";
    }
}

static const char *severity_name(v2v_severity_t s)
{
    switch (s)
    {
        case V2V_SEVERITY_WARNING:
            return "WARNING";
        case V2V_SEVERITY_CRITICAL:
            return "CRITICAL";
        default:
            return "INFO";
    }
}

/* One decimal without printf float support (debug console may lack %f). */
static const char *fmt1(char *buf, size_t n, float x)
{
    int v           = (int)((x >= 0.0f) ? (x * 10.0f + 0.5f) : (x * 10.0f - 0.5f));
    const char *sgn = (v < 0) ? "-" : "";
    if (v < 0)
    {
        v = -v;
    }
    (void)snprintf(buf, n, "%s%d.%d", sgn, v / 10, v % 10);
    return buf;
}

#if APP_ADC_SCAN_AT_BOOT
/* OPEN-014: turn the TPS pot during the scan and see which channel moves. */
static void adc_scan_at_boot(void)
{
    for (uint32_t pass = 0u; pass < APP_ADC_SCAN_PASSES; pass++)
    {
        char line[160];
        int len = snprintf(line, sizeof line, "adc_scan pass=%u", (unsigned)pass);
        for (uint8_t ch = 0u; (ch < BOARD_ADC_SCAN_CHANNELS) && (len > 0) && ((size_t)len < sizeof line); ch++)
        {
            int32_t raw;
            if (board_adc_read_channel_raw(ch, &raw))
            {
                len += snprintf(&line[len], sizeof line - (size_t)len, " ch%u=%d", (unsigned)ch, (int)raw);
            }
            else
            {
                len += snprintf(&line[len], sizeof line - (size_t)len, " ch%u=ERR", (unsigned)ch);
            }
        }
        board_log("%s", line);
        if ((pass + 1u) < APP_ADC_SCAN_PASSES)
        {
            board_delay_us(APP_ADC_SCAN_PERIOD_MS * 1000u);
        }
    }
}
#endif

#if APP_TIMEBASE_SELFTEST
/* Bring-up: is the CTIMER1 microsecond timebase really 1 MHz in THIS project (wifi_mqtt clocks)? */
static void timebase_selftest(void)
{
    char line[192];
    uint32_t ctimer_hz;
    uint32_t prescale;
    uint32_t ticks;
    uint32_t cycles;
    uint32_t core_hz;

    board_timebase_info(&ctimer_hz, &prescale);
    if (board_timebase_measure(1000000u, &ticks, &cycles, &core_hz) && (core_hz >= 1000000u))
    {
        (void)snprintf(line, sizeof line,
                       "timebase ctimer_hz=%u prescale=%u delay_1s: ctimer_ticks=%u core_cycles=%u core_hz=%u core_us=%u",
                       (unsigned)ctimer_hz, (unsigned)prescale, (unsigned)ticks, (unsigned)cycles, (unsigned)core_hz,
                       (unsigned)(cycles / (core_hz / 1000000u)));
    }
    else
    {
        (void)snprintf(line, sizeof line, "timebase ctimer_hz=%u prescale=%u delay_1s: no DWT cycle counter",
                       (unsigned)ctimer_hz, (unsigned)prescale);
    }
    board_log("%s", line);

    board_log("%s", "timebase blink: green LED 10 x (500 ms on + 500 ms off) = 10 s, time it");
    for (uint32_t i = 0u; i < APP_TIMEBASE_BLINKS; i++)
    {
        leds_set_status(true, false, false);
        board_delay_us(500000u);
        leds_set_status(false, false, false);
        board_delay_us(500000u);
    }
    board_log("%s", "timebase blink: done");
}
#endif

/* ------------------------------------------------------------------------- */
/* sense task: the MP1 loop                                                  */
/* ------------------------------------------------------------------------- */

static void sense_task(void *arg)
{
    (void)arg;

    alert_ctx_t ctx;
    alert_init(&ctx, NULL);

#if APP_ENABLE_BRAKE_FIR
    static fir_t brake_fir;
    static float brake_fir_buf[FIR_BRAKE_N];
    (void)fir_init(&brake_fir, fir_brake_h, brake_fir_buf, (uint16_t)FIR_BRAKE_N);
    float brake_x = 0.0f; /* last raw brake %, for the log */
    float brake_y = 0.0f; /* last FIR output %, for the log */
#endif

    const pot_t pot_brake = {BOARD_ADC_BRAKE, APP_POT_BRAKE_RAW_MIN, APP_POT_BRAKE_RAW_MAX};
    const pot_t pot_tps   = {BOARD_ADC_TPS, APP_POT_TPS_RAW_MIN, APP_POT_TPS_RAW_MAX};

    telemetry_t tlm   = {0};
    alert_t alert     = {V2V_ALERT_NONE, V2V_SEVERITY_INFO, false};
    bool temp_valid   = false;
    float temp_c      = 0.0f;
    bool has_tps      = false;
    bool dht_busy     = false;
    float dist_cm     = hcsr04_read_cm(); /* first reading before the loop */
    int32_t brake_raw = 0;
    int dht_st        = (int)DHT11_BUSY; /* last finished DHT11 attempt; BUSY = none yet */

    uint32_t dht_ok = 0u, dht_cks = 0u, dht_to = 0u, dht_nr = 0u;
    uint32_t slot_late_max = 0u, slot_resyncs = 0u;
    int32_t adc_int[BOARD_ADC_INTERNAL_COUNT] = {INT32_MIN, INT32_MIN, INT32_MIN};
    unsigned diag_slot     = 0u;
    uint16_t alert_seq                = 0u;
    v2v_alert_type_t last_alert_type  = V2V_ALERT_NONE;
    v2v_severity_t last_alert_sev     = V2V_SEVERITY_INFO;

    uint32_t now      = board_time_ms();
    uint32_t t_sensor = now;
    uint32_t t_hcsr04 = now;
    uint32_t t_dht    = now; /* first DHT11 read after one period (>= 1 s after power-up) */

    for (;;)
    {
        now = board_time_ms();

        /* DHT11: start when due, then poll until it reports. */
        if (!dht_busy && ((uint32_t)(now - t_dht) >= APP_DHT11_PERIOD_MS))
        {
            t_dht                = now;
            dht11_status_t start = dht11_start(now);
            dht_busy             = (start == DHT11_BUSY);
            if (!dht_busy)
            {
                dht_st = (int)start; /* e.g. DHT11_ERR_TOO_SOON; not counted as a read failure */
            }
        }
        if (dht_busy)
        {
            float t = 0.0f;
#if APP_DHT11_FRAME_CRITICAL
            taskENTER_CRITICAL();
#endif
            dht11_status_t st = dht11_poll(board_time_ms(), &t);
#if APP_DHT11_FRAME_CRITICAL
            taskEXIT_CRITICAL();
#endif
            if (st != DHT11_BUSY)
            {
                dht_busy   = false;
                dht_st     = (int)st;
                temp_valid = (st == DHT11_OK);
                switch (st)
                {
                    case DHT11_OK:
                        dht_ok++;
                        temp_c = t;
                        break;
                    case DHT11_ERR_CHECKSUM:
                        dht_cks++;
                        break;
                    case DHT11_ERR_TIMEOUT:
                        dht_to++;
                        break;
                    case DHT11_ERR_NO_RESPONSE:
                        dht_nr++;
                        break;
                    default:
                        break;
                }
            }
        }

        /* Pots -> decision. Runs before the blocking HC-SR04 read (DEC-010) and keeps a fixed grid:
         * the next slot is t_sensor + period, not now + period, so a late sample does not delay every
         * later one. More than one period late (e.g. after a DHT11 read): resynchronise to now. */
        if (!dht_busy && ((uint32_t)(now - t_sensor) >= APP_SENSOR_PERIOD_MS))
        {
            uint32_t late = (uint32_t)(now - t_sensor) - APP_SENSOR_PERIOD_MS;
            if (late > slot_late_max)
            {
                slot_late_max = late;
            }
            t_sensor += APP_SENSOR_PERIOD_MS;
            if ((uint32_t)(now - t_sensor) >= APP_SENSOR_PERIOD_MS)
            {
                t_sensor = now;
                slot_resyncs++;
            }

            float brake = 0.0f;
            float tps   = 0.0f;
            uint8_t ok  = 0u;

            if (dist_cm >= 0.0f)
            {
                ok |= ALERT_SENSOR_DIST;
            }
            if (pot_read(&pot_brake, &brake, &brake_raw))
            {
                ok |= ALERT_SENSOR_BRAKE;
#if APP_ENABLE_BRAKE_FIR
                /* Only successful reads enter the filter. Until N samples are in (fir_ready) the raw
                 * value is used, as in MP1 (DEC-009). */
                brake_x = brake;
                brake_y = fir_process(&brake_fir, brake);
                if (fir_ready(&brake_fir))
                {
                    brake = brake_y;
                }
#endif
            }
#if APP_ADC_DIAG
            if ((diag_slot++ % APP_ADC_DIAG_EVERY) == 0u)
            {
                board_adc_read_internal(adc_int);
            }
#else
            (void)diag_slot;
#endif
            has_tps = pot_read(&pot_tps, &tps, NULL);
            if (!has_tps)
            {
                tps = 0.0f; /* TPS is optional: no sensor_ok bit */
            }
            if (temp_valid)
            {
                ok |= ALERT_SENSOR_TEMP;
            }

            tlm.ts_ms       = board_time_ms();
            tlm.distance_cm = dist_cm;
            tlm.temp_c      = temp_c;
            tlm.brake_pct   = brake;
            tlm.tps_pct     = tps;
            tlm.sensor_ok   = ok;

            (void)alert_evaluate(&ctx, &tlm, &alert);

            /* Mirror every NEW alert to the tlm task; never block (local path first). */
            if ((alert.type != V2V_ALERT_NONE) && ((alert.type != last_alert_type) || (alert.severity != last_alert_sev)))
            {
                alert_event_t ev = {tlm.ts_ms, alert_seq++, alert.type, alert.severity};
                if (xQueueSend(s_alert_q, &ev, 0) != pdTRUE)
                {
                    s_alert_drops++;
                }
            }
            last_alert_type = alert.type;
            last_alert_sev  = alert.severity;

            snapshot_t snap = {0};
            snap.ts_ms                 = tlm.ts_ms;
            snap.brake_pct             = tlm.brake_pct;
            snap.temp_c                = tlm.temp_c;
            snap.distance_cm           = tlm.distance_cm;
            snap.tps_pct               = tlm.tps_pct;
            snap.temp_valid            = temp_valid;
            snap.dist_valid            = (ok & ALERT_SENSOR_DIST) != 0u;
            snap.has_tps               = has_tps;
            snap.sensor_ok             = ok;
            snap.alert_type            = alert.type;
            snap.alert_sev             = alert.severity;
            snap.sensor_fault          = alert.sensor_fault;
#if APP_ENABLE_BRAKE_FIR
            snap.brake_raw_pct         = brake_x;
            snap.brake_filtered_pct    = brake_y;
#else
            snap.brake_raw_pct         = tlm.brake_pct;
            snap.brake_filtered_pct    = tlm.brake_pct;
#endif
            snap.brake_raw             = brake_raw;
            snap.dht_st                = dht_st;
            snap.dht_ok                = dht_ok;
            snap.dht_cks               = dht_cks;
            snap.dht_to                = dht_to;
            snap.dht_nr                = dht_nr;
            snap.echo_us               = hcsr04_last_echo_us();
            snap.echo_err              = (int)hcsr04_last_err();
            snap.slot_late_max_ms      = slot_late_max;
            snap.slot_resyncs          = slot_resyncs;
            snap.rem_active            = alert_actuation_remote_active(&snap.rem_sev);
            for (unsigned i = 0u; i < BOARD_ADC_INTERNAL_COUNT; i++)
            {
                snap.adc_int[i] = adc_int[i];
            }
            (void)xQueueOverwrite(s_snap_q, &snap);
        }

        /* HC-SR04 on its own schedule; the last distance is reused in between. */
        if (!dht_busy && ((uint32_t)(now - t_hcsr04) >= APP_HCSR04_PERIOD_MS))
        {
            t_hcsr04 = now;
            dist_cm  = hcsr04_read_cm();
        }

        /* Dashboard commands (DEC-013): non-blocking; they only add a remote alert on top of the local one. */
        alert_cmd_t rc;
        while (xQueueReceive(s_remote_q, &rc, 0) == pdTRUE)
        {
            alert_actuation_remote(&rc, board_time_ms());
        }

        alert_actuation_update(&alert, tlm.tps_pct, board_time_ms());

        vTaskDelay(pdMS_TO_TICKS(APP_SENSE_YIELD_MS));
    }
}

/* ------------------------------------------------------------------------- */
/* tlm task: Wi-Fi, MQTT, publishing, log line                               */
/* ------------------------------------------------------------------------- */

static void log_line(const snapshot_t *s, const tlm_mqtt_stats_t *m)
{
    char b_dist[16], b_temp[16], b_brake[16], b_brake_f[16], b_tps[16];
    char line[640];
    char nif[48];
    tlm_mqtt_netif_text(nif, sizeof nif);
    (void)snprintf(line, sizeof line,
                   "t=%u dist=%s temp=%s brake=%s brake_f=%s brake_raw=%d tps=%s alert=%s sev=%s fault=%d"
                   " echo_us=%u echo_err=%d dht_st=%d dht_ok=%u dht_cks=%u dht_to=%u dht_nr=%u"
                   " slot_late_max=%u resyncs=%u wf=%d wf_dc=%u mq=%d mq_dc=%u mq_rc=%u pub_ok=%u pub_err=%u sen_ok=%u sen_err=%u"
                   " al_drop=%u heap=%u heap_min=%u rssi=%d rem=%s sub=%u cmd_rx=%u cmd_ok=%u cmd_bad=%u cmd_dup=%u cmd_qd=%u"
#if APP_ADC_DIAG
                   " adc_vref=%d adc_vbat=%d adc_vss=%d"
#endif
                   " %s",
                   (unsigned)s->ts_ms, fmt1(b_dist, sizeof b_dist, s->distance_cm),
                   s->temp_valid ? fmt1(b_temp, sizeof b_temp, s->temp_c) : "NA",
                   fmt1(b_brake, sizeof b_brake, s->brake_raw_pct), fmt1(b_brake_f, sizeof b_brake_f, s->brake_filtered_pct),
                   (int)s->brake_raw, s->has_tps ? fmt1(b_tps, sizeof b_tps, s->tps_pct) : "NA", alert_name(s->alert_type),
                   severity_name(s->alert_sev), s->sensor_fault ? 1 : 0, s->echo_us, s->echo_err, s->dht_st,
                   (unsigned)s->dht_ok, (unsigned)s->dht_cks, (unsigned)s->dht_to, (unsigned)s->dht_nr,
                   (unsigned)s->slot_late_max_ms, (unsigned)s->slot_resyncs, tlm_mqtt_wifi_up() ? 1 : 0,
                   (unsigned)m->wifi_drops, tlm_mqtt_connected() ? 1 : 0, (unsigned)m->mqtt_drops,
                   (unsigned)m->mqtt_reconnects, (unsigned)m->pub_ok, (unsigned)m->pub_err, (unsigned)m->sens_ok, (unsigned)m->sens_err,
                   (unsigned)s_alert_drops,
                   (unsigned)xPortGetFreeHeapSize(), (unsigned)xPortGetMinimumEverFreeHeapSize(), tlm_mqtt_wifi_rssi(),
                   s->rem_active ? severity_name(s->rem_sev) : "-",
                   (unsigned)m->sub_ok, (unsigned)m->cmd_rx, (unsigned)s_cmd_ok, (unsigned)s_cmd_bad, (unsigned)s_cmd_dup,
                   (unsigned)(m->cmd_q_drop + s_remote_drops),
#if APP_ADC_DIAG
                   (int)s->adc_int[0], (int)s->adc_int[1], (int)s->adc_int[2],
#endif
                   nif);
    board_log("%s", line);
}

/** Drain the commands received on v2v/{node}/cmd and v2v/all/cmd: log, de-duplicate, hand over to the sense task. */
static void handle_commands(void)
{
    tlm_mqtt_cmd_t m;
    while (tlm_mqtt_cmd_poll(&m))
    {
        const char *topic = m.broadcast ? "all" : "node";
        if (m.status != ALERT_CMD_OK)
        {
            s_cmd_bad++;
            board_log("cmd: ignored %s topic=%s", alert_cmd_status_name(m.status), topic);
            continue;
        }
        if (alert_cmd_dedupe_seen(&s_dedupe, &m.cmd))
        {
            s_cmd_dup++;
            board_log("cmd: duplicate origin=%s seq=%u topic=%s", m.cmd.origin, (unsigned)m.cmd.seq, topic);
            continue;
        }
        s_cmd_ok++;
        if (m.cmd.kind == ALERT_CMD_ALERT)
        {
            board_log("cmd: alert %s %s %ums seq=%u origin=%s topic=%s%s%s",
                      severity_name(m.cmd.severity), alert_name(m.cmd.alert_type), (unsigned)m.cmd.duration_ms,
                      (unsigned)m.cmd.seq, m.cmd.origin, topic, (m.cmd.source_node[0] != '\0') ? " source=" : "",
                      m.cmd.source_node);
        }
        else
        {
            board_log("cmd: clear seq=%u origin=%s topic=%s", (unsigned)m.cmd.seq, m.cmd.origin, topic);
        }
        if (xQueueSend(s_remote_q, &m.cmd, 0) != pdTRUE)
        {
            s_remote_drops++;
        }
    }
}

static void publish_telemetry(const snapshot_t *s, uint32_t seq)
{
    char json[256];
    tlm_json_telemetry_t t = {0};
    t.node_id              = APP_NODE_ID;
    t.ts_ms                = s->ts_ms;
    t.seq                  = seq;
    t.brake_pct            = s->brake_pct;
    t.temp_valid           = s->temp_valid;
    t.temp_c               = s->temp_c;
    t.dist_valid           = s->dist_valid;
    t.distance_cm          = s->distance_cm;
    t.has_tps              = s->has_tps;
    t.tps_pct              = s->tps_pct;
    t.has_sensor_ok        = true;
    t.sensor_ok            = s->sensor_ok;
    int n                  = tlm_json_telemetry(json, sizeof json, &t);
    if (n > 0)
    {
        (void)tlm_mqtt_publish_telemetry(json, (uint16_t)n);
    }
}

/**
 * Per-variable topics (plain text, QoS 0). "null" only on a sensor fault (sensor_ok bit missing); an HC-SR04
 * out-of-range reading is valid (DEC-008) and is published as a number (e.g. "500.0").
 */
static void publish_sensor_topics(const snapshot_t *s)
{
    char txt[16];
    int n = tlm_json_value1(txt, sizeof txt, (s->sensor_ok & ALERT_SENSOR_BRAKE) != 0u, s->brake_pct);
    if (n > 0)
    {
        (void)tlm_mqtt_publish_sensor(TLM_SENSOR_BRAKE_PCT, txt, (uint16_t)n);
    }
    n = tlm_json_value1(txt, sizeof txt, s->temp_valid, s->temp_c);
    if (n > 0)
    {
        (void)tlm_mqtt_publish_sensor(TLM_SENSOR_TEMP_C, txt, (uint16_t)n);
    }
    n = tlm_json_value1(txt, sizeof txt, s->dist_valid, s->distance_cm);
    if (n > 0)
    {
        (void)tlm_mqtt_publish_sensor(TLM_SENSOR_DISTANCE_CM, txt, (uint16_t)n);
    }
}

/** Publish queued alerts in order; an event leaves the queue only after mqtt_publish accepted it. */
static void drain_alerts(void)
{
    alert_event_t ev;
    while (tlm_mqtt_connected() && (xQueuePeek(s_alert_q, &ev, 0) == pdTRUE))
    {
        char json[160];
        int n = tlm_json_alert(json, sizeof json, APP_NODE_ID, ev.ts_ms, ev.seq, ev.type, ev.severity);
        if (n <= 0)
        {
            (void)xQueueReceive(s_alert_q, &ev, 0); /* cannot be formatted: drop it */
            continue;
        }
        if (!tlm_mqtt_publish_alert(json, (uint16_t)n))
        {
            break; /* buffer or in-flight table full: retry on the next tick */
        }
        (void)xQueueReceive(s_alert_q, &ev, 0);
    }
}

/** One-off boot print of every task's priority (verifies the Wi-Fi/tcpip priorities the README relies on). */
static void log_task_priorities(void)
{
    static TaskStatus_t st[24];
    UBaseType_t n = uxTaskGetSystemState(st, (UBaseType_t)(sizeof st / sizeof st[0]), NULL);
    char line[256];
    int len = snprintf(line, sizeof line, "tasks(name:prio)");
    for (UBaseType_t i = 0u; (i < n) && (len > 0) && ((size_t)len < sizeof line); i++)
    {
        len += snprintf(&line[len], sizeof line - (size_t)len, " %s:%u", st[i].pcTaskName,
                        (unsigned)st[i].uxCurrentPriority);
    }
    board_log("%s", line);
}

static void tlm_task(void *arg)
{
    (void)arg;

    if (!tlm_mqtt_wifi_join(&s_net_cfg) || !tlm_mqtt_start())
    {
        board_log("%s", "tlm: network start failed, node keeps running locally without MQTT");
        vTaskDelete(NULL);
    }

    log_task_priorities();

    uint32_t seq            = 0u;
    uint32_t tick           = 0u;
    const uint32_t per_tlm  = APP_TLM_PERIOD_MS / APP_TLM_TICK_MS;
    const uint32_t per_sens = APP_SENSOR_TOPIC_PERIOD_MS / APP_TLM_TICK_MS;
    TickType_t last_wake    = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_TLM_TICK_MS));
        tlm_mqtt_service();
        drain_alerts();
        handle_commands();

        tick++;
#if APP_SENSOR_TOPICS
        if (((tick % per_sens) == 1u) && tlm_mqtt_connected())
        {
            snapshot_t sens_snap;
            if (xQueuePeek(s_snap_q, &sens_snap, 0) == pdTRUE)
            {
                publish_sensor_topics(&sens_snap);
            }
        }
#else
        (void)per_sens;
#endif
        if ((tick % per_tlm) != 0u)
        {
            continue;
        }
        snapshot_t snap;
        if (xQueuePeek(s_snap_q, &snap, 0) == pdTRUE)
        {
            if (tlm_mqtt_connected())
            {
                publish_telemetry(&snap, seq++);
            }
            tlm_mqtt_stats_t m;
            tlm_mqtt_get_stats(&m);
            log_line(&snap, &m);
        }
    }
}

/* ------------------------------------------------------------------------- */

int main(void)
{
    board_init_all();
    hcsr04_init();
    dht11_init();
    alert_actuation_init((APP_ENABLE_SHIFT_LIGHT != 0) ? (uint8_t)APP_SHIFT_LEDS : 0u);

    board_log("%s", "MP2 node rw612-a: local sense -> decide -> actuate + Wi-Fi/MQTT (FreeRTOS)");
#if APP_TIMEBASE_SELFTEST
    timebase_selftest();
#endif
#if APP_ADC_SCAN_AT_BOOT
    adc_scan_at_boot();
#endif
#if APP_ENABLE_BRAKE_FIR
    {
        char b_fs[16];
        char b_fc[16];
        unsigned delay_ms = (unsigned)(((float)(FIR_BRAKE_N - 1u) * 500.0f / FIR_BRAKE_FS_HZ) + 0.5f);
        board_log("fir N=%u fs=%s fc=%s delay_ms=%u", (unsigned)FIR_BRAKE_N, fmt1(b_fs, sizeof b_fs, FIR_BRAKE_FS_HZ),
                  fmt1(b_fc, sizeof b_fc, FIR_BRAKE_FC_HZ), delay_ms);
        if ((unsigned)((1000.0f / FIR_BRAKE_FS_HZ) + 0.5f) != (unsigned)APP_SENSOR_PERIOD_MS)
        {
            board_log("fir WARNING: coefficients designed for fs=%s Hz but APP_SENSOR_PERIOD_MS=%u",
                      fmt1(b_fs, sizeof b_fs, FIR_BRAKE_FS_HZ), (unsigned)APP_SENSOR_PERIOD_MS);
        }
    }
#endif

    alert_cmd_dedupe_init(&s_dedupe);
    s_snap_q   = xQueueCreate(1, sizeof(snapshot_t));
    s_alert_q  = xQueueCreate(APP_ALERT_QUEUE_LEN, sizeof(alert_event_t));
    s_remote_q = xQueueCreate(APP_REMOTE_QUEUE_LEN, sizeof(alert_cmd_t));
    if ((s_snap_q == NULL) || (s_alert_q == NULL) || (s_remote_q == NULL) ||
        #if !APP_DIAG_NO_SENSE
        (xTaskCreate(sense_task, "sense", APP_SENSE_STACK_WORDS, NULL, APP_SENSE_PRIO, NULL) != pdPASS) ||
#endif
        (xTaskCreate(tlm_task, "tlm", APP_TLM_STACK_WORDS, NULL, APP_TLM_PRIO, NULL) != pdPASS))
    {
        board_log("%s", "FATAL: queue/task creation failed (heap?)");
        for (;;)
        {
        }
    }
    board_log("boot: sense prio=%u tlm prio=%u heap_free=%u", (unsigned)APP_SENSE_PRIO, (unsigned)APP_TLM_PRIO,
              (unsigned)xPortGetFreeHeapSize());

    vTaskStartScheduler();

    /* Not reached unless the scheduler could not start (no heap for the idle task). */
    for (;;)
    {
    }
}
