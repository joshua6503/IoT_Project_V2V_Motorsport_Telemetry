/**
 * node_io.cpp - see node_io.h.
 */
#include <Arduino.h>

#include "config.h"
#include "node_io.h"

/* Buzzer type: 1 = passive (needs a tone, LEDC PWM), 0 = active (plain on/off). */
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE 0
#endif
#ifndef BUZZER_TONE_HZ
#define BUZZER_TONE_HZ 2500
#endif
#ifndef BUZZER_SELFTEST
#define BUZZER_SELFTEST 0
#endif

static uint32_t s_feedback_start_ms = 0;
static bool     s_feedback_active   = false;

/* Button debounce state (active low, INPUT_PULLUP). */
static bool     s_btn_stable_down = false;
static bool     s_btn_last_raw    = false;
static uint32_t s_btn_change_ms   = 0;

/** Sound the buzzer: 50 % duty tone at BUZZER_TONE_HZ (passive) or DC (active). */
static void buzzer_set(bool on)
{
#if BUZZER_PASSIVE
    /* 8-bit resolution: 128/256 = 50 % duty. Log a failure once, not every loop. */
    static bool s_logged_fail = false;
    bool        ok            = ledcWrite(PIN_BUZZER, on ? 128 : 0);
    if (!ok && !s_logged_fail)
    {
        s_logged_fail = true;
        Serial.println("[buzz] ledcWrite FAILED (pin not attached to LEDC?)");
    }
#else
    digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
#endif
}

void io_init(void)
{
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
#if BUZZER_PASSIVE
    bool attached = ledcAttach(PIN_BUZZER, BUZZER_TONE_HZ, 8); /* Arduino-ESP32 3.x LEDC API: pin, freq, bits */
    Serial.printf("[buzz] passive on GPIO%d, ledcAttach(%u Hz, 8 bit) %s\n", (int)PIN_BUZZER,
                  (unsigned)BUZZER_TONE_HZ, attached ? "ok" : "FAILED");
#else
    pinMode(PIN_BUZZER, OUTPUT);
#endif
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    buzzer_set(false);

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    analogReadResolution(12);
    /* 12 dB attenuation (ADC_11db is the Arduino alias of ADC_ATTEN_DB_12). The
     * usable full-scale (about 3.1 V) is an ASSUMPTION, not sourced: verify on the bench. */
    analogSetPinAttenuation(PIN_BRAKE_POT, ADC_11db);
}

uint32_t io_read_brake_mv(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < BRAKE_AVG_SAMPLES; i++)
    {
        sum += analogReadMilliVolts(PIN_BRAKE_POT);
    }
    return sum / BRAKE_AVG_SAMPLES;
}

bool io_brake_mv_valid(uint32_t mv)
{
    return (mv >= BRAKE_MV_FAULT_LO) && (mv <= BRAKE_MV_FAULT_HI);
}

float io_brake_pct_from_mv(uint32_t mv)
{
    float pct = ((float)mv - (float)BRAKE_MV_MIN) * 100.0f / (float)(BRAKE_MV_MAX - BRAKE_MV_MIN);
    if (pct < 0.0f)
    {
        pct = 0.0f;
    }
    if (pct > 100.0f)
    {
        pct = 100.0f;
    }
    return pct;
}

bool io_button_pressed(uint32_t now_ms)
{
    bool raw = (digitalRead(PIN_BUTTON) == LOW);
    if (raw != s_btn_last_raw)
    {
        s_btn_last_raw  = raw;
        s_btn_change_ms = now_ms;
    }
    if ((raw != s_btn_stable_down) && ((uint32_t)(now_ms - s_btn_change_ms) >= BUTTON_DEBOUNCE_MS))
    {
        s_btn_stable_down = raw;
        return raw; /* true only on the press edge */
    }
    return false;
}

void io_manual_feedback(uint32_t now_ms)
{
    s_feedback_start_ms = now_ms;
    s_feedback_active   = true;
}

void io_apply(const alert_pattern_t *p, uint32_t now_ms)
{
    bool yellow = p->led_yellow;
    bool buzzer = p->buzzer_on;

    if (s_feedback_active)
    {
        uint32_t el = (uint32_t)(now_ms - s_feedback_start_ms);
        if (el >= MANUAL_FEEDBACK_MS)
        {
            s_feedback_active = false;
        }
        else
        {
            /* Yellow on in even blink steps, short beep at the start. */
            if (((el / MANUAL_FEEDBACK_BLINK_MS) % 2u) == 0u)
            {
                yellow = true;
            }
            if (el < MANUAL_FEEDBACK_BEEP_MS)
            {
                buzzer = true;
            }
        }
    }

    digitalWrite(PIN_LED_GREEN, p->led_green ? HIGH : LOW);
    digitalWrite(PIN_LED_YELLOW, yellow ? HIGH : LOW);
    digitalWrite(PIN_LED_RED, p->led_red ? HIGH : LOW);
    buzzer_set(buzzer);
}

#if BUZZER_SELFTEST
/* TEMPORARY bench aid, blocks at boot on purpose. BUZZER_SELFTEST in config.h:
 *   1 = sweep 1000-5000 Hz in 500 Hz steps (0.5 s each) then 3 s at 2500 Hz
 *   2 = continuous BUZZER_TONE_HZ tone, 50 % duty, forever, a line every 5 s (scope)
 *   3 = plain digital GPIO test: 2 s HIGH, 2 s LOW, forever, LEDC detached meanwhile
 * Modes 2 and 3 never return: reset or reflash to leave them. */
static void selftest_restore(void)
{
#if BUZZER_PASSIVE
    ledcWriteTone(PIN_BUZZER, 0);
    uint32_t cf = ledcChangeFrequency(PIN_BUZZER, BUZZER_TONE_HZ, 8); /* back to the normal PWM setup */
    Serial.printf("[buzz] SELFTEST done, ledcChangeFrequency(%u) returned %lu\n", (unsigned)BUZZER_TONE_HZ, (unsigned long)cf);
#endif
    buzzer_set(false);
}

void io_buzzer_selftest(void)
{
#if BUZZER_PASSIVE
#if BUZZER_SELFTEST == 1
    Serial.println("[buzz] SELFTEST 1 start: 1000-5000 Hz in 500 Hz steps, 0.5 s each, then 3 s at 2500 Hz");
    for (uint32_t f = 1000; f <= 5000; f += 500)
    {
        uint32_t r = ledcWriteTone(PIN_BUZZER, f);
        Serial.printf("[buzz] %4lu Hz -> ledcWriteTone returned %lu, duty %lu %s\n", (unsigned long)f,
                      (unsigned long)r, (unsigned long)ledcRead(PIN_BUZZER), (r != 0) ? "ok" : "FAIL");
        delay(500);
    }
    uint32_t r = ledcWriteTone(PIN_BUZZER, 2500);
    Serial.printf("[buzz] 2500 Hz for 3 s -> ledcWriteTone returned %lu %s\n", (unsigned long)r, (r != 0) ? "ok" : "FAIL");
    delay(3000);
    selftest_restore();
#elif BUZZER_SELFTEST == 2
    uint32_t r = ledcWriteTone(PIN_BUZZER, BUZZER_TONE_HZ);
    Serial.printf("[buzz] SELFTEST 2 start: %u Hz continuous on GPIO%d, until reset, ledcWriteTone returned %lu duty %lu (50 %% = 511 of 1023)\n",
                  (unsigned)BUZZER_TONE_HZ, (int)PIN_BUZZER, (unsigned long)r, (unsigned long)ledcRead(PIN_BUZZER));
    for (int t = 5;; t += 5)
    {
        delay(5000);
        Serial.printf("[buzz] tone running, t=%d s, duty %lu\n", t, (unsigned long)ledcRead(PIN_BUZZER));
    }
    selftest_restore();
#elif BUZZER_SELFTEST == 3
    bool det = ledcDetach(PIN_BUZZER);
    pinMode(PIN_BUZZER, OUTPUT);
    Serial.printf("[buzz] SELFTEST 3 start: digital GPIO%d, 2 s HIGH / 2 s LOW until reset, ledcDetach %s\n", (int)PIN_BUZZER, det ? "ok" : "FAILED");
    for (int i = 0;; i++)
    {
        digitalWrite(PIN_BUZZER, HIGH);
        Serial.printf("[buzz] HIGH (cycle %d)\n", i + 1);
        delay(2000);
        digitalWrite(PIN_BUZZER, LOW);
        Serial.printf("[buzz] LOW  (cycle %d)\n", i + 1);
        delay(2000);
    }
    bool att = ledcAttach(PIN_BUZZER, BUZZER_TONE_HZ, 8);
    Serial.printf("[buzz] SELFTEST 3 done, ledcAttach %s\n", att ? "ok" : "FAILED");
    buzzer_set(false);
#else
    Serial.println("[buzz] SELFTEST: unknown mode (use 1, 2 or 3)");
#endif
#else
    Serial.println("[buzz] SELFTEST skipped: BUZZER_PASSIVE is 0");
#endif
}
#endif
