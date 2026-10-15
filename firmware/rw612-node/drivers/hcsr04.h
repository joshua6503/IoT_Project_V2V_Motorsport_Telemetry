/**
 * @file hcsr04.h
 * @brief HC-SR04 ultrasonic distance sensor (TRIG/ECHO), polled with the µs timebase.
 */
#ifndef HCSR04_H
#define HCSR04_H

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum time between measurements: "measurement cycle > 60 ms" [HCSR04.pdf, SparkFun]. */
#define HCSR04_MIN_CYCLE_MS 60u

/** Reading returned when the echo is still high at the timeout: out of range (DEC-008). */
#define HCSR04_MAX_CM 500.0f

/** Configure TRIG as output (low) and ECHO as input. */
void hcsr04_init(void);

/**
 * @brief Trigger one measurement and wait for the echo (blocks up to ~35 ms).
 *
 * DEC-008:
 * - no rising edge on ECHO within 5 ms (or ECHO already high) -> -1.0f, sensor fault;
 * - ECHO still high after 30 ms -> HCSR04_MAX_CM, a valid "nothing in range" reading.
 *
 * Call at most every HCSR04_MIN_CYCLE_MS.
 * @return Distance in cm, HCSR04_MAX_CM if out of range, or -1.0f on fault.
 */
float hcsr04_read_cm(void);

#ifdef __cplusplus
}
#endif

#endif /* HCSR04_H */
