/**
 * @file hcsr04.c
 * @brief HC-SR04 driver. See hcsr04.h.
 *
 * Datasheet: HCSR04.pdf (SparkFun) — TRIG 10 us TTL pulse; distance in cm = us / 58;
 * range 2 cm - 4 m; measurement cycle > 60 ms.
 */
#include "hcsr04.h"

#include <stdbool.h>
#include <stdint.h>

#include "board_io.h"

#define HCSR04_TRIG_PULSE_US 10u   /**< "10uS TTL pulse" [HCSR04.pdf] */
#define HCSR04_US_PER_CM     58.0f /**< "uS / 58 = centimeters" [HCSR04.pdf] */
/** No ECHO rising edge within this time -> sensor fault (DEC-008). */
#define HCSR04_ECHO_START_TIMEOUT_US 5000u
/** ECHO still high after this -> out of range (DEC-008); 4 m max range [HCSR04.pdf] + margin. */
#define HCSR04_ECHO_TIMEOUT_US 30000u

static uint32_t s_echo_us;
static hcsr04_err_t s_err = HCSR04_ERR_NO_RISE;

void hcsr04_init(void)
{
    board_gpio_set_output(BOARD_IO_HCSR04_TRIG, false);
    board_gpio_set_input(BOARD_IO_HCSR04_ECHO);
}

static bool wait_level(bool level, uint32_t timeout_us, uint32_t *edge_us)
{
    uint32_t t0 = board_time_us();
    while (board_gpio_read(BOARD_IO_HCSR04_ECHO) != level)
    {
        if ((board_time_us() - t0) > timeout_us)
        {
            return false;
        }
    }
    *edge_us = board_time_us();
    return true;
}

float hcsr04_read_cm(void)
{
    uint32_t rise_us;
    uint32_t fall_us;

    s_echo_us = 0u;

    /* A previous echo still high: the sensor is not ready. */
    if (board_gpio_read(BOARD_IO_HCSR04_ECHO))
    {
        s_err = HCSR04_ERR_BUSY;
        return -1.0f;
    }

    board_gpio_write(BOARD_IO_HCSR04_TRIG, true);
    board_delay_us(HCSR04_TRIG_PULSE_US);
    board_gpio_write(BOARD_IO_HCSR04_TRIG, false);

    if (!wait_level(true, HCSR04_ECHO_START_TIMEOUT_US, &rise_us))
    {
        s_err = HCSR04_ERR_NO_RISE;
        return -1.0f; /* no answer: fault */
    }
    if (!wait_level(false, HCSR04_ECHO_TIMEOUT_US, &fall_us))
    {
        s_echo_us = board_time_us() - rise_us;
        s_err     = HCSR04_ERR_TIMEOUT;
        return HCSR04_MAX_CM; /* nothing in range: valid reading */
    }
    s_echo_us = fall_us - rise_us;
    s_err     = HCSR04_ERR_OK;
    return (float)s_echo_us / HCSR04_US_PER_CM;
}

unsigned hcsr04_last_echo_us(void)
{
    return (unsigned)s_echo_us;
}

hcsr04_err_t hcsr04_last_err(void)
{
    return s_err;
}
