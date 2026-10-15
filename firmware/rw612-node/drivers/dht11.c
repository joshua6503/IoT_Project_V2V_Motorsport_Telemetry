/**
 * @file dht11.c
 * @brief DHT11 driver. See dht11.h.
 *
 * Datasheet: DHT11, Aosong (MP1-conexiones/datasheets/DHT11_datasheet.pdf), printed pages:
 *   p.3 sampling period > 2 s; p.4 frame = RH int, RH dec, T int, T dec, checksum (sum of
 *   the first 4), decimal part is 0; p.5 start low >= 18 ms, response 80 us low + 80 us high,
 *   wait 1 s after power-up; p.6 bit = 50 us low, then 26-28 us high ("0") or 70 us high ("1").
 */
#include "dht11.h"

#include <stdbool.h>
#include <stddef.h>

#include "board_io.h"

#define DHT11_START_LOW_MS     20u  /**< host start pulse, ">= 18 ms" [p.5] */
#define DHT11_RESPONSE_TIMEOUT 100u /**< us per edge; longest phase is 80 us [p.5] */
#define DHT11_BIT1_THRESHOLD   50u  /**< us high: "0" = 26-28 us, "1" = 70 us [p.6] */
#define DHT11_FRAME_BYTES      5u
#define DHT11_POWERUP_MS       1000u /**< wait after power-up before the first read [p.5] */

typedef enum
{
    DHT11_STATE_IDLE = 0,
    DHT11_STATE_START_LOW
} dht11_state_t;

static dht11_state_t s_state;
static uint32_t s_start_ms;
static uint32_t s_last_read_ms;
static bool s_has_read;

void dht11_init(void)
{
    board_gpio_set_input(BOARD_IO_DHT11);
    s_state    = DHT11_STATE_IDLE;
    s_has_read = false;
}

dht11_status_t dht11_start(uint32_t now_ms)
{
    if (s_state != DHT11_STATE_IDLE)
    {
        return DHT11_BUSY;
    }
    if (s_has_read ? ((uint32_t)(now_ms - s_last_read_ms) < DHT11_MIN_INTERVAL_MS)
                   : (now_ms < DHT11_POWERUP_MS)) /* now_ms counts from board_init_all() */
    {
        return DHT11_ERR_TOO_SOON;
    }
    board_gpio_set_output(BOARD_IO_DHT11, false);
    s_start_ms = now_ms;
    s_state    = DHT11_STATE_START_LOW;
    return DHT11_BUSY;
}

/** Wait while the line is at @p level; return the time spent (us) or -1 on timeout. */
static int32_t wait_while(bool level, uint32_t timeout_us)
{
    uint32_t t0 = board_time_us();
    while (board_gpio_read(BOARD_IO_DHT11) == level)
    {
        if ((board_time_us() - t0) > timeout_us)
        {
            return -1;
        }
    }
    return (int32_t)(board_time_us() - t0);
}

static dht11_status_t read_frame(uint8_t data[DHT11_FRAME_BYTES])
{
    /* Release the line; the pull-up takes it high and the sensor answers
     * with ~80 us low + ~80 us high before the first bit. */
    board_gpio_set_input(BOARD_IO_DHT11);

    if (wait_while(false, DHT11_RESPONSE_TIMEOUT) < 0) /* line rises after the release */
    {
        return DHT11_ERR_NO_RESPONSE;
    }
    if (wait_while(true, DHT11_RESPONSE_TIMEOUT) < 0) /* sensor pulls low */
    {
        return DHT11_ERR_NO_RESPONSE;
    }
    if (wait_while(false, DHT11_RESPONSE_TIMEOUT) < 0) /* response low */
    {
        return DHT11_ERR_NO_RESPONSE;
    }
    if (wait_while(true, DHT11_RESPONSE_TIMEOUT) < 0) /* response high */
    {
        return DHT11_ERR_NO_RESPONSE;
    }

    for (uint32_t i = 0u; i < (DHT11_FRAME_BYTES * 8u); i++)
    {
        if (wait_while(false, DHT11_RESPONSE_TIMEOUT) < 0) /* bit start (low) */
        {
            return DHT11_ERR_TIMEOUT;
        }
        int32_t high_us = wait_while(true, DHT11_RESPONSE_TIMEOUT); /* bit value (high) */
        if (high_us < 0)
        {
            return DHT11_ERR_TIMEOUT;
        }
        data[i / 8u] = (uint8_t)(data[i / 8u] << 1);
        if ((uint32_t)high_us > DHT11_BIT1_THRESHOLD)
        {
            data[i / 8u] |= 1u;
        }
    }

    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    return (sum == data[4]) ? DHT11_OK : DHT11_ERR_CHECKSUM;
}

dht11_status_t dht11_poll(uint32_t now_ms, float *temp_c)
{
    if ((s_state != DHT11_STATE_START_LOW) || ((uint32_t)(now_ms - s_start_ms) < DHT11_START_LOW_MS))
    {
        return DHT11_BUSY;
    }

    uint8_t data[DHT11_FRAME_BYTES] = {0u, 0u, 0u, 0u, 0u};
    dht11_status_t st = read_frame(data);

    board_gpio_set_input(BOARD_IO_DHT11);
    s_state        = DHT11_STATE_IDLE;
    s_last_read_ms = now_ms;
    s_has_read     = true;

    if ((st == DHT11_OK) && (temp_c != NULL))
    {
        /* T int + T dec / 10; the decimal byte is 0 on the DHT11 [p.4]. */
        *temp_c = (float)data[2] + ((float)data[3] / 10.0f);
    }
    return st;
}
