/**
 * @file buzzer.h
 * @brief Buzzer, active (GPIO on/off) or passive (square wave) per BUZZER_KIND in app_config.h.
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Configure the output, buzzer off. */
void buzzer_init(void);

/** Turn the buzzer on/off. Repeated calls with the same value do nothing. */
void buzzer_set(bool on);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_H */
