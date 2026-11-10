/**
 * @file fir_filter.h
 * @brief FIR filter as a discrete convolution y[n] = sum_k h[k] * x[n-k] (REQ-F-015, DEC-009).
 *
 * Pure C99: no SDK, RTOS or I/O. The caller owns the coefficient array and the
 * delay-line buffer (both of length n); the filter keeps the last n inputs in a
 * circular buffer. The initial state is zero, so the first n-1 outputs are the
 * filter's warm-up transient (see fir_ready()).
 */
#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** FIR state. Treat as opaque; use the fir_* functions. */
typedef struct
{
    const float *h; /**< coefficients h[0..n-1], owned by the caller */
    float *buf;     /**< delay line x[n-0..n-(n-1)], n floats owned by the caller */
    uint16_t n;     /**< number of taps */
    uint16_t idx;   /**< buf index where the next input is written */
    uint16_t count; /**< inputs pushed since init/reset, saturates at n */
} fir_t;

/**
 * @brief Bind @p f to coefficients and a buffer, then reset it.
 * @param f    Filter state.
 * @param h    n coefficients (kept by reference, must outlive @p f).
 * @param buf  n floats of working memory (kept by reference).
 * @param n    Number of taps, > 0.
 * @return false if any pointer is NULL or n == 0 (@p f is left untouched).
 */
bool fir_init(fir_t *f, const float *h, float *buf, uint16_t n);

/** @brief Zero the delay line and restart the warm-up. NULL is ignored. */
void fir_reset(fir_t *f);

/**
 * @brief Push one input sample and return the filtered output.
 *
 * Computes y[n] = sum_{k=0}^{N-1} h[k] * x[n-k], with x[m] = 0 for m < 0
 * (zero initial state).
 *
 * @param f  Initialised filter; NULL returns 0.
 * @param x  New input sample x[n].
 * @return   y[n].
 */
float fir_process(fir_t *f, float x);

/**
 * @brief True once n samples have been pushed since init/reset.
 *
 * From then on every output uses only real inputs (no zero initial state).
 */
bool fir_ready(const fir_t *f);

#ifdef __cplusplus
}
#endif

#endif /* FIR_FILTER_H */
