/**
 * @file test_fir_filter.c
 * @brief Unity tests for firmware/common fir_filter with the generated brake coefficients
 *        (REQ-F-015, DEC-009).
 */
#include <stddef.h>

#include "unity.h"

#include "fir_brake_coeffs.h"
#include "fir_filter.h"
#include "fir_reference.h"

#define TOL_STEP   1e-4f /* step settles to sum(h) ~ 1.0 */
#define TOL_GOLDEN 1e-5f /* C float vs numpy.convolve */
#define TOL_EXACT  1e-7f

static fir_t fir;
static float buf[FIR_BRAKE_N];

void setUp(void)
{
    TEST_ASSERT_TRUE(fir_init(&fir, fir_brake_h, buf, (uint16_t)FIR_BRAKE_N));
}

void tearDown(void) {}

static float sum_h(void)
{
    float s = 0.0f;
    for (uint16_t k = 0u; k < FIR_BRAKE_N; k++)
    {
        s += fir_brake_h[k];
    }
    return s;
}

/* ------------------------------------------------------------------------- */

static void test_fir_init_rejects_null_and_zero_taps(void)
{
    fir_t f;
    float b[4];
    static const float h[4] = {0.25f, 0.25f, 0.25f, 0.25f};

    TEST_ASSERT_FALSE(fir_init(NULL, h, b, 4u));
    TEST_ASSERT_FALSE(fir_init(&f, NULL, b, 4u));
    TEST_ASSERT_FALSE(fir_init(&f, h, NULL, 4u));
    TEST_ASSERT_FALSE(fir_init(&f, h, b, 0u));
    TEST_ASSERT_TRUE(fir_init(&f, h, b, 4u));
}

static void test_fir_init_zeroes_buffer(void)
{
    fir_t f;
    float b[3] = {7.0f, -3.0f, 1e6f};
    static const float h[3] = {1.0f, 2.0f, 3.0f};

    TEST_ASSERT_TRUE(fir_init(&f, h, b, 3u));
    for (int i = 0; i < 3; i++)
    {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, b[i]);
    }
    /* Zero initial state: first output is h[0] * x[0] only. */
    TEST_ASSERT_FLOAT_WITHIN(TOL_EXACT, 2.0f, fir_process(&f, 2.0f));
}

static void test_fir_null_is_safe(void)
{
    fir_reset(NULL);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fir_process(NULL, 1.0f));
    TEST_ASSERT_FALSE(fir_ready(NULL));
}

static void test_fir_impulse_gives_coefficients_then_zeros(void)
{
    for (uint16_t n = 0u; n < 3u * FIR_BRAKE_N; n++)
    {
        float y        = fir_process(&fir, (n == 0u) ? 1.0f : 0.0f);
        float expected = (n < FIR_BRAKE_N) ? fir_brake_h[n] : 0.0f;
        TEST_ASSERT_FLOAT_WITHIN(TOL_EXACT, expected, y);
    }
}

static void test_fir_step_settles_to_dc_gain(void)
{
    float y = 0.0f;
    for (uint16_t n = 0u; n < 2u * FIR_BRAKE_N; n++)
    {
        y = fir_process(&fir, 1.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(TOL_STEP, sum_h(), y);
    TEST_ASSERT_FLOAT_WITHIN(TOL_STEP, 1.0f, y); /* firwin low-pass: DC gain 1 */
}

static void test_fir_ready_after_n_samples(void)
{
    for (uint16_t n = 0u; n < FIR_BRAKE_N; n++)
    {
        TEST_ASSERT_FALSE(fir_ready(&fir));
        (void)fir_process(&fir, 1.0f);
    }
    TEST_ASSERT_TRUE(fir_ready(&fir));
    (void)fir_process(&fir, 1.0f);
    TEST_ASSERT_TRUE(fir_ready(&fir)); /* count saturates, stays ready */
}

static void test_fir_reset_clears_state(void)
{
    for (uint16_t n = 0u; n < FIR_BRAKE_N + 3u; n++)
    {
        (void)fir_process(&fir, 5.0f);
    }
    fir_reset(&fir);
    TEST_ASSERT_FALSE(fir_ready(&fir));
    /* Behaves like a fresh filter: impulse response from scratch. */
    TEST_ASSERT_FLOAT_WITHIN(TOL_EXACT, fir_brake_h[0], fir_process(&fir, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOL_EXACT, fir_brake_h[1], fir_process(&fir, 0.0f));
}

static void test_fir_matches_numpy_convolve_golden(void)
{
    TEST_ASSERT_EQUAL_UINT(FIR_BRAKE_N, FIR_REF_N); /* reference regenerated with the same taps */
    for (uint16_t n = 0u; n < FIR_REF_LEN; n++)
    {
        float y = fir_process(&fir, fir_ref_x[n]);
        TEST_ASSERT_FLOAT_WITHIN(TOL_GOLDEN, fir_ref_y[n], y);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fir_init_rejects_null_and_zero_taps);
    RUN_TEST(test_fir_init_zeroes_buffer);
    RUN_TEST(test_fir_null_is_safe);
    RUN_TEST(test_fir_impulse_gives_coefficients_then_zeros);
    RUN_TEST(test_fir_step_settles_to_dc_gain);
    RUN_TEST(test_fir_ready_after_n_samples);
    RUN_TEST(test_fir_reset_clears_state);
    RUN_TEST(test_fir_matches_numpy_convolve_golden);
    return UNITY_END();
}
