/**
 * @file main_mp1.c
 * @brief Mini Proyecto 1: local sense -> decide -> actuate, bare-metal superloop (DEC-001).
 *
 * No network: the node reads its sensors, runs alert_evaluate() and drives its own
 * LEDs/buzzer (REQ-F-014). Replaces hello_world.c in the imported SDK project.
 *
 * Boot: optional ADC scan CH0..CH7 (APP_ADC_SCAN_AT_BOOT, OPEN-014).
 * Loop (non-blocking except the bounded sensor reads), periods per DEC-006, in this
 * order within each iteration:
 *   1. every APP_DHT11_PERIOD_MS: start a DHT11 read; while it runs, the pot and
 *      HC-SR04 slots are skipped so the HC-SR04 wait cannot stretch the DHT11 start pulse.
 *   2. every APP_SENSOR_PERIOD_MS: pots -> telemetry_t -> alert_evaluate(). Fixed grid
 *      (t_sensor += period), resynchronised to now if more than one period late (DEC-010).
 *      It runs before the HC-SR04 so the blocking echo wait does not delay the sample.
 *   3. every APP_HCSR04_PERIOD_MS: HC-SR04, blocking up to ~35 ms (DEC-008); the last
 *      distance is reused in between (the pot slot uses the previous one).
 *   4. every iteration: alert_actuation_update() (smooth blinking).
 *   5. every APP_LOG_PERIOD_MS: one log line on the debug UART.
 *
 * PDS (DEC-009, REQ-F-015/016): with APP_ENABLE_BRAKE_FIR the brake pot goes through
 * an FIR low-pass (fir_filter.c) before alert_evaluate(); with APP_LOG_DSP_CSV every
 * brake sample is printed as `dsp,<t_ms>,<x_pct>,<y_pct>` instead of the log line.
 * With APP_ENABLE_BRAKE_FIR 0 none of this is compiled and MP1 is unchanged.
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

#if APP_ENABLE_BRAKE_FIR
#include "fir_brake_coeffs.h"
#include "fir_filter.h"
#endif

#if APP_LOG_DSP_CSV && !APP_ENABLE_BRAKE_FIR
#error "APP_LOG_DSP_CSV needs APP_ENABLE_BRAKE_FIR 1 (it prints the raw and filtered brake)"
#endif

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

#if APP_ENABLE_BRAKE_FIR
    static fir_t brake_fir;
    static float brake_fir_buf[FIR_BRAKE_N];
    (void)fir_init(&brake_fir, fir_brake_h, brake_fir_buf, (uint16_t)FIR_BRAKE_N);
    {
        char b_fs[16];
        char b_fc[16];
        /* Group delay of a linear-phase FIR: (N-1)/(2*fs). Logged, not hidden (DEC-009). */
        unsigned delay_ms = (unsigned)(((float)(FIR_BRAKE_N - 1u) * 500.0f / FIR_BRAKE_FS_HZ) + 0.5f);
        board_log("fir N=%u fs=%s fc=%s delay_ms=%u", (unsigned)FIR_BRAKE_N,
                  fmt1(b_fs, sizeof b_fs, FIR_BRAKE_FS_HZ), fmt1(b_fc, sizeof b_fc, FIR_BRAKE_FC_HZ), delay_ms);
        if ((unsigned)((1000.0f / FIR_BRAKE_FS_HZ) + 0.5f) != (unsigned)APP_SENSOR_PERIOD_MS)
        {
            board_log("fir WARNING: coefficients designed for fs=%s Hz but APP_SENSOR_PERIOD_MS=%u",
                      fmt1(b_fs, sizeof b_fs, FIR_BRAKE_FS_HZ), (unsigned)APP_SENSOR_PERIOD_MS);
        }
    }
    float brake_x = 0.0f; /* last raw brake %, for the log */
    float brake_y = 0.0f; /* last FIR output %, for the log */
#endif

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

        /* Pots -> decision. Runs before the blocking HC-SR04 read (DEC-010) and keeps
         * a fixed grid: the next slot is t_sensor + period, not now + period, so a late
         * sample does not delay every later one. More than one period late (e.g. after
         * a DHT11 read): resynchronise to now instead of a burst of catch-up samples. */
        if (!dht_busy && ((uint32_t)(now - t_sensor) >= APP_SENSOR_PERIOD_MS))
        {
            t_sensor += APP_SENSOR_PERIOD_MS;
            if ((uint32_t)(now - t_sensor) >= APP_SENSOR_PERIOD_MS)
            {
                t_sensor = now;
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
                /* Only successful reads enter the filter: a failed pot_read pushes no sample.
                 * Warm-up choice: until N samples are in (fir_ready), the output still
                 * contains the zero initial state and would read low, so alert_evaluate()
                 * gets the raw value for those first (N-1)/fs seconds (400 ms at N=9, 20 Hz). */
                brake_x = brake;
                brake_y = fir_process(&brake_fir, brake);
                if (fir_ready(&brake_fir))
                {
                    brake = brake_y;
                }
#if APP_LOG_DSP_CSV
                {
                    char b_x[16];
                    char b_y[16];
                    board_log("dsp,%u,%s,%s", (unsigned)board_time_ms(), fmt1(b_x, sizeof b_x, brake_x),
                              fmt1(b_y, sizeof b_y, brake_y));
                }
#endif
#endif
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

        /* HC-SR04 on its own schedule; the last distance is reused in between. */
        if (!dht_busy && ((uint32_t)(now - t_hcsr04) >= APP_HCSR04_PERIOD_MS))
        {
            t_hcsr04 = now;
            dist_cm  = hcsr04_read_cm();
        }

        now = board_time_ms();
        alert_actuation_update(&alert, tlm.tps_pct, now);

#if !APP_LOG_DSP_CSV /* DSP capture mode: only the dsp, lines go out (clean capture) */
        if ((uint32_t)(now - t_log) >= APP_LOG_PERIOD_MS)
        {
            t_log = now;
            char b_dist[16];
            char b_temp[16];
            char b_brake[16];
            char b_tps[16];
#if APP_ENABLE_BRAKE_FIR
            char line[224]; /* room for brake_f= */
#else
            char line[192];
#endif
            uint32_t adc32;
            uint8_t fifo;
            board_adc_last_diag(&adc32, &fifo);
#if APP_ENABLE_BRAKE_FIR
            /* brake= raw pot %, brake_f= FIR output (what alert_evaluate() sees once warmed up). */
            char b_brake_f[16];
            (void)snprintf(line, sizeof line,
                           "t=%u dist=%s temp=%s brake=%s brake_f=%s brake_raw=%d tps=%s alert=%s sev=%s fault=%d"
                           " adc32=0x%08x fifo=%u echo_us=%u echo_err=%d dht_st=%d",
                           (unsigned)now, fmt1(b_dist, sizeof b_dist, tlm.distance_cm),
                           temp_valid ? fmt1(b_temp, sizeof b_temp, temp_c) : "NA",
                           fmt1(b_brake, sizeof b_brake, brake_x), fmt1(b_brake_f, sizeof b_brake_f, brake_y),
                           (int)brake_raw, fmt1(b_tps, sizeof b_tps, tlm.tps_pct), alert_name(alert.type),
                           severity_name(alert.severity), alert.sensor_fault ? 1 : 0, (unsigned)adc32,
                           (unsigned)fifo, hcsr04_last_echo_us(), (int)hcsr04_last_err(), dht_st);
#else
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
#endif
            board_log("%s", line);
        }
#else
        (void)t_log;
        (void)dht_st;
        (void)alert_name;
        (void)severity_name;
#endif
    }
}
