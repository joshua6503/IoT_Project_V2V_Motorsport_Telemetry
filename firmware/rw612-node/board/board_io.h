/**
 * @file board_io.h
 * @brief Thin, SDK-free interface over the RW612 peripherals used by MP1.
 *
 * Upper layers address pins by logical id (board_io_t), never by number; the
 * mapping lives in board_pins.h. Only board_io.c includes fsl_*.h.
 */
#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Upper bound for the shift-light bar; the fitted count is board_shift_led_count(). */
#define BOARD_SHIFT_LED_MAX 8u

/** Logical GPIO lines. */
typedef enum
{
    BOARD_IO_LED_GREEN = 0,
    BOARD_IO_LED_YELLOW,
    BOARD_IO_LED_RED,
    BOARD_IO_BUZZER,
    BOARD_IO_HCSR04_TRIG,
    BOARD_IO_HCSR04_ECHO,
    BOARD_IO_DHT11,
    BOARD_IO_SHIFT_0, /**< BOARD_IO_SHIFT_0 + i, i < board_shift_led_count() */
    BOARD_IO_COUNT = BOARD_IO_SHIFT_0 + BOARD_SHIFT_LED_MAX
} board_io_t;

/** Logical ADC inputs. */
typedef enum
{
    BOARD_ADC_BRAKE = 0,
    BOARD_ADC_TPS,
    BOARD_ADC_COUNT
} board_adc_t;

/** Pins, clocks, debug console, GPIO, ADC and timebase. Call once, first. */
void board_init_all(void);

/** Configure @p io as output and drive @p level. */
void board_gpio_set_output(board_io_t io, bool level);
/** Configure @p io as input (external pull-up/down as wired). */
void board_gpio_set_input(board_io_t io);
/** Drive an output previously configured with board_gpio_set_output(). */
void board_gpio_write(board_io_t io, bool level);
/** Read the level of @p io. */
bool board_gpio_read(board_io_t io);

/** LED polarity from board_pins.h: true if a high level lights the LED. */
bool board_led_active_high(void);
/** Number of shift-light LEDs fitted (0..BOARD_SHIFT_LED_MAX). */
uint8_t board_shift_led_count(void);

/**
 * @brief One blocking single-shot ADC conversion.
 * @param[out] raw Raw conversion result (format: see TODO(tutor) in board_io.c).
 * @return false on timeout or invalid arguments.
 */
bool board_adc_read_raw(board_adc_t ch, int32_t *raw);

/** Number of single-ended ADC channels board_adc_read_channel_raw() accepts (CH0..CH7). */
#define BOARD_ADC_SCAN_CHANNELS 8u

/**
 * @brief Raw conversion of single-ended channel @p channel (0..7), for the boot scan (OPEN-014).
 * @return false on timeout, invalid channel or NULL @p raw.
 */
bool board_adc_read_channel_raw(uint8_t channel, int32_t *raw);

/** Free-running microsecond counter (wraps after ~71 min; use unsigned differences). */
uint32_t board_time_us(void);
/** Milliseconds since board_init_all() (wraps after ~49 days). */
uint32_t board_time_ms(void);
/** Busy-wait at least @p us microseconds. */
void board_delay_us(uint32_t us);

/** Start a 50 % square wave at @p freq_hz on the buzzer pin (passive buzzer). */
void board_tone_start(uint32_t freq_hz);
/** Stop the square wave. */
void board_tone_stop(void);

/** printf-style line to the debug console (UART over MCU-Link). */
void board_log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_IO_H */
