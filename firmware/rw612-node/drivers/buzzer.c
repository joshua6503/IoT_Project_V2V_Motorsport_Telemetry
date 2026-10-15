/**
 * @file buzzer.c
 * @brief Buzzer driver. See buzzer.h.
 *
 * The output drives the buzzer through an NPN transistor (hardware/bom.md), so a
 * high level / running square wave means "on". ASSUMPTION until HANDOFF-002.
 */
#include "buzzer.h"

#include "app_config.h"
#include "board_io.h"

#if (BUZZER_KIND != BUZZER_KIND_ACTIVE) && (BUZZER_KIND != BUZZER_KIND_PASSIVE)
#error "BUZZER_KIND must be BUZZER_KIND_ACTIVE or BUZZER_KIND_PASSIVE (app_config.h)"
#endif

static bool s_on;

static void apply(bool on)
{
#if BUZZER_KIND == BUZZER_KIND_ACTIVE
    board_gpio_write(BOARD_IO_BUZZER, on);
#else
    if (on)
    {
        board_tone_start(BUZZER_PASSIVE_HZ);
    }
    else
    {
        board_tone_stop();
    }
#endif
}

void buzzer_init(void)
{
#if BUZZER_KIND == BUZZER_KIND_ACTIVE
    board_gpio_set_output(BOARD_IO_BUZZER, false);
#endif
    s_on = false;
    apply(false);
}

void buzzer_set(bool on)
{
    if (on != s_on)
    {
        s_on = on;
        apply(on);
    }
}
