/**
 * @file leds.c
 * @brief LED driver. See leds.h.
 */
#include "leds.h"

#include "board_io.h"

static uint8_t s_n_shift;

static void led_write(board_io_t io, bool on)
{
    /* Shift LEDs share the status LEDs' polarity (BOARD_LED_ACTIVE_HIGH). */
    board_gpio_write(io, on == board_led_active_high());
}

void leds_init(uint8_t n_shift)
{
    bool off = !board_led_active_high();

    board_gpio_set_output(BOARD_IO_LED_GREEN, off);
    board_gpio_set_output(BOARD_IO_LED_YELLOW, off);
    board_gpio_set_output(BOARD_IO_LED_RED, off);

    s_n_shift = (n_shift < board_shift_led_count()) ? n_shift : board_shift_led_count();
    for (uint8_t i = 0u; i < s_n_shift; i++)
    {
        board_gpio_set_output((board_io_t)(BOARD_IO_SHIFT_0 + i), off);
    }
}

void leds_set_status(bool green, bool yellow, bool red)
{
    led_write(BOARD_IO_LED_GREEN, green);
    led_write(BOARD_IO_LED_YELLOW, yellow);
    led_write(BOARD_IO_LED_RED, red);
}

void leds_set_shift(uint8_t level)
{
    for (uint8_t i = 0u; i < s_n_shift; i++)
    {
        led_write((board_io_t)(BOARD_IO_SHIFT_0 + i), i < level);
    }
}

uint8_t leds_shift_count(void)
{
    return s_n_shift;
}
