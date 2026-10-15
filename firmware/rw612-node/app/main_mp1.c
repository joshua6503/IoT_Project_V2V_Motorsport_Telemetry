/**
 * @file main_mp1.c
 * @brief Mini Proyecto 1: local sense -> decide -> actuate, bare-metal superloop (DEC-001).
 *
 * No network: the node reads its sensors, runs alert_evaluate() and drives its own
 * LEDs/buzzer (REQ-F-014). Replaces hello_world.c in the imported SDK project.
 *
 * Boot: optional ADC scan CH0..CH7 (APP_ADC_SCAN_AT_BOOT, OPEN-014).
 * Loop (non-blocking except the bounded sensor reads), schedules per DEC-006:
 *   - every APP_DHT11_PERIOD_MS: start a DHT11 read; while it runs, the sensor
 *     reads are skipped so the HC-SR04 wait cannot stretch the DHT11 start pulse.
 *   - every APP_HCSR04_PERIOD_MS: HC-SR04 (last distance reused in between).
 *   - every APP_SENSOR_PERIOD_MS: pots -> telemetry_t -> alert_evaluate().
 *   - every iteration: alert_actuation_update() (smooth blinking).
 *   - every APP_LOG_PERIOD_MS: one log line on the debug UART.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "alert_actuation.h"
#include "alert_logic.h"
#include "app_config.h"
#include "board_io.h"
#include "dht11.h"
#include "hcsr04.h"
#include "leds.h"
#include "pot.h"

/* Sensor timing limits from the datasheets, enforced at build time (DEC-006). */
#if APP_HCSR04_PERIOD_MS < HCSR04_MIN_CYCLE_MS
#error "APP_HCSR04_PERIOD_MS below the HC-SR04 measurement cycle (HCSR04_MIN_CYCLE_MS)"
#endif
#if APP_DHT11_PERIOD_MS < DHT11_MIN_INTERVAL_MS
#error "APP_DHT11_PERIOD_MS below the DHT11 sampling period (DHT11_MIN_INTERVAL_MS)"
#endif

static const char *alert_name(v2v_alert_type_t t)
{
    switch (t)
    {
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
/* Bring-up: is the CTIMER1 microsecond timebase really 1 MHz? */
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
        /* core_us: the 1 s delay measured with the core clock, independent of CTIMER1. */
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

int main(void)
{
    board_init_all();
    hcsr04_init();
    dht11_init();
    alert_actuation_init((APP_ENABLE_SHIFT_LIGHT != 0) ? (uint8_t)APP_SHIFT_LEDS : 0u);

    board_log("%s", "MP1 perception node: local sense -> decide -> actuate");
#if APP_TIMEBASE_SELFTEST
    timebase_selftest();
#endif
#if APP_ADC_SCAN_AT_BOOT
    adc_scan_at_boot();
#endif

    alert_ctx_t ctx;
    alert_init(&ctx, NULL);

    const pot_t pot_brake = {BOARD_ADC_BRAKE, APP_POT_BRAKE_RAW_MIN, APP_POT_BRAKE_RAW_MAX};
    const pot_t pot_tps   = {BOARD_ADC_TPS, APP_POT_TPS_RAW_MIN, APP_POT_TPS_RAW_MAX};

    telemetry_t tlm   = {0};
    alert_t alert     = {V2V_ALERT_NONE, V2V_SEVERITY_INFO, false};
    bool temp_valid   = false;
    float temp_c      = 0.0f;
    bool dht_busy     = false;
    float dist_cm     = hcsr04_read_cm(); /* first reading before the loop */
    int32_t brake_raw = 0;
    int dht_st        = (int)DHT11_BUSY; /* last finished DHT11 attempt; BUSY = none yet */

    uint32_t now      = board_time_ms();
    uint32_t t_sensor = now;
    uint32_t t_hcsr04 = now;
    uint32_t t_dht    = now; /* first DHT11 read after one period (>= 1 s after power-up) */
    uint32_t t_log    = now;

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
                dht_st = (int)start; /* e.g. DHT11_ERR_TOO_SOON */
            }
        }
        if (dht_busy)
        {
            float t           = 0.0f;
            dht11_status_t st = dht11_poll(board_time_ms(), &t);
            if (st != DHT11_BUSY)
            {
                dht_busy   = false;
                dht_st     = (int)st;
                temp_valid = (st == DHT11_OK);
                if (temp_valid)
                {
                    temp_c = t;
                }
            }
        }

        /* HC-SR04 on its own schedule; the last distance is reused in between. */
        if (!dht_busy && ((uint32_t)(now - t_hcsr04) >= APP_HCSR04_PERIOD_MS))
        {
            t_hcsr04 = now;
            dist_cm  = hcsr04_read_cm();
        }

        /* Pots -> decision. */
        if (!dht_busy && ((uint32_t)(now - t_sensor) >= APP_SENSOR_PERIOD_MS))
        {
            t_sensor = now;

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
            }
            if (!pot_read(&pot_tps, &tps, NULL))
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
        }

        now = board_time_ms();
        alert_actuation_update(&alert, tlm.tps_pct, now);

        if ((uint32_t)(now - t_log) >= APP_LOG_PERIOD_MS)
        {
            t_log = now;
            char b_dist[16];
            char b_temp[16];
            char b_brake[16];
            char b_tps[16];
            char line[192];
            uint32_t adc32;
            uint8_t fifo;
            board_adc_last_diag(&adc32, &fifo);
            /* snprintf with %s/%d/%u/%x only, no length modifiers (int is 32-bit here). */
            (void)snprintf(line, sizeof line,
                           "t=%u dist=%s temp=%s brake=%s brake_raw=%d tps=%s alert=%s sev=%s fault=%d"
                           " adc32=0x%08x fifo=%u echo_us=%u echo_err=%d dht_st=%d",
                           (unsigned)now, fmt1(b_dist, sizeof b_dist, tlm.distance_cm),
                           temp_valid ? fmt1(b_temp, sizeof b_temp, temp_c) : "NA",
                           fmt1(b_brake, sizeof b_brake, tlm.brake_pct), (int)brake_raw,
                           fmt1(b_tps, sizeof b_tps, tlm.tps_pct), alert_name(alert.type),
                           severity_name(alert.severity), alert.sensor_fault ? 1 : 0, (unsigned)adc32,
                           (unsigned)fifo, hcsr04_last_echo_us(), (int)hcsr04_last_err(), dht_st);
            board_log("%s", line);
        }
    }
}
