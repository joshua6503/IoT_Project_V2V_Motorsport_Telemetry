/**
 * node_io.cpp - see node_io.h.
 */
#include <Arduino.h>

#include "config.h"
#include "node_io.h"

static uint32_t s_feedback_start_ms = 0;
static bool     s_feedback_active   = false;

/* Button debounce state (active low, INPUT_PULLUP). */
static bool     s_btn_stable_down = false;
static bool     s_btn_last_raw    = false;
static uint32_t s_btn_change_ms   = 0;

void io_init(void)
{
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_BUZZER, LOW);

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
    digitalWrite(PIN_BUZZER, buzzer ? HIGH : LOW);
}
