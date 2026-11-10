/**
 * @file fir_filter.c
 * @brief FIR discrete convolution with a circular delay line. See fir_filter.h.
 */
#include "fir_filter.h"

#include <stddef.h>

bool fir_init(fir_t *f, const float *h, float *buf, uint16_t n)
{
    if ((f == NULL) || (h == NULL) || (buf == NULL) || (n == 0u))
    {
        return false;
    }
    f->h   = h;
    f->buf = buf;
    f->n   = n;
    fir_reset(f);
    return true;
}

void fir_reset(fir_t *f)
{
    if (f == NULL)
    {
        return;
    }
    for (uint16_t i = 0u; i < f->n; i++)
    {
        f->buf[i] = 0.0f;
    }
    f->idx   = 0u;
    f->count = 0u;
}

float fir_process(fir_t *f, float x)
{
    if ((f == NULL) || (f->buf == NULL) || (f->h == NULL) || (f->n == 0u))
    {
        return 0.0f;
    }

    /* Store x[n] at idx; then buf[idx - k] (mod n) holds x[n-k]. */
    uint16_t newest = f->idx;
    f->buf[newest]  = x;
    f->idx          = (uint16_t)((newest + 1u == f->n) ? 0u : (newest + 1u));
    if (f->count < f->n)
    {
        f->count++;
    }

    /* Two runs instead of a modulo per tap: k = 0..newest walks buf[newest..0],
     * k = newest+1..n-1 walks buf[n-1..newest+1]. */
    float y    = 0.0f;
    uint16_t k = 0u;
    for (int32_t i = (int32_t)newest; i >= 0; i--, k++)
    {
        y += f->h[k] * f->buf[i];
    }
    for (int32_t i = (int32_t)f->n - 1; i > (int32_t)newest; i--, k++)
    {
        y += f->h[k] * f->buf[i];
    }
    return y;
}

bool fir_ready(const fir_t *f)
{
    return (f != NULL) && (f->n > 0u) && (f->count >= f->n);
}
