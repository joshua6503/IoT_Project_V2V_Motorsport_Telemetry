/**
 * @file test_tlm_json.c
 * @brief Unity tests for firmware/common tlm_json: golden strings for the MQTT payloads
 *        of interfaces/mqtt-topics.md (telemetry, alert, status).
 */
#include <math.h>
#include <string.h>

#include "unity.h"

#include "tlm_json.h"

#define NODE "rw612-a"

void setUp(void) {}
void tearDown(void) {}

static tlm_json_telemetry_t nominal(void)
{
    tlm_json_telemetry_t t = {0};
    t.node_id              = NODE;
    t.ts_ms                = 123456u;
    t.seq                  = 812u;
    t.brake_pct            = 42.34f;
    t.temp_valid           = true;
    t.temp_c               = 36.8f;
    t.dist_valid           = true;
    t.distance_cm          = 250.0f;
    return t;
}

/* ------------------------------------------------------------------------- */
/* telemetry                                                                 */
/* ------------------------------------------------------------------------- */

static void test_telemetry_nominal_without_optionals(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    const char *want =
        "{\"node_id\":\"rw612-a\",\"ts_ms\":123456,\"seq\":812,\"brake_pct\":42.3,\"temp_c\":36.8,\"distance_cm\":250.0}";
    int n = tlm_json_telemetry(buf, sizeof buf, &t);
    TEST_ASSERT_EQUAL_INT((int)strlen(want), n);
    TEST_ASSERT_EQUAL_STRING(want, buf);
}

static void test_telemetry_with_tps_and_sensor_ok(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    t.has_tps              = true;
    t.tps_pct              = 7.0f;
    t.has_sensor_ok        = true;
    t.sensor_ok            = 7u;
    const char *want = "{\"node_id\":\"rw612-a\",\"ts_ms\":123456,\"seq\":812,\"brake_pct\":42.3,\"temp_c\":36.8,"
                       "\"distance_cm\":250.0,\"tps_pct\":7.0,\"sensor_ok\":7}";
    TEST_ASSERT_EQUAL_INT((int)strlen(want), tlm_json_telemetry(buf, sizeof buf, &t));
    TEST_ASSERT_EQUAL_STRING(want, buf);
}

static void test_telemetry_null_on_sensor_fault(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    t.temp_valid           = false;
    t.dist_valid           = false;
    const char *want       = "{\"node_id\":\"rw612-a\",\"ts_ms\":123456,\"seq\":812,\"brake_pct\":42.3,\"temp_c\":null,"
                             "\"distance_cm\":null}";
    TEST_ASSERT_EQUAL_INT((int)strlen(want), tlm_json_telemetry(buf, sizeof buf, &t));
    TEST_ASSERT_EQUAL_STRING(want, buf);
}

static void test_telemetry_nan_values_become_null_or_zero(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    t.temp_c               = NAN;
    t.distance_cm          = INFINITY;
    t.brake_pct            = NAN;
    TEST_ASSERT_TRUE(tlm_json_telemetry(buf, sizeof buf, &t) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"brake_pct\":0.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp_c\":null"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"distance_cm\":null"));
}

static void test_telemetry_negative_and_rounding(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    t.temp_c               = -5.26f;  /* -> -5.3 */
    t.distance_cm          = 99.96f;  /* -> 100.0 (carry) */
    t.brake_pct            = 0.04f;   /* -> 0.0 */
    TEST_ASSERT_TRUE(tlm_json_telemetry(buf, sizeof buf, &t) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp_c\":-5.3"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"distance_cm\":100.0"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"brake_pct\":0.0,"));
}

static void test_telemetry_small_negative_has_no_minus_zero(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    t.temp_c               = -0.04f;
    TEST_ASSERT_TRUE(tlm_json_telemetry(buf, sizeof buf, &t) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp_c\":0.0"));
    TEST_ASSERT_NULL(strstr(buf, "-0.0"));
}

static void test_telemetry_extreme_values_do_not_overflow(void)
{
    char buf[256];
    tlm_json_telemetry_t t = nominal();
    t.ts_ms                = 0xFFFFFFFFu;
    t.seq                  = 0xFFFFFFFFu;
    t.distance_cm          = 3.0e38f;
    t.temp_c               = -3.0e38f;
    TEST_ASSERT_TRUE(tlm_json_telemetry(buf, sizeof buf, &t) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ts_ms\":4294967295,\"seq\":4294967295"));
}

static void test_telemetry_fits_the_mqtt_ring_buffer(void)
{
    /* Worst case must stay well below MQTT_OUTPUT_RINGBUF_SIZE (1024, see rw612-node README). */
    char buf[512];
    tlm_json_telemetry_t t = nominal();
    t.ts_ms                = 0xFFFFFFFFu;
    t.seq                  = 0xFFFFFFFFu;
    t.brake_pct            = 100.0f;
    t.temp_c               = -40.0f;
    t.distance_cm          = 400.0f;
    t.has_tps              = true;
    t.tps_pct              = 100.0f;
    t.has_sensor_ok        = true;
    t.sensor_ok            = 255u;
    int n                  = tlm_json_telemetry(buf, sizeof buf, &t);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_TRUE(n < 220);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
}

/* ------------------------------------------------------------------------- */
/* alert                                                                     */
/* ------------------------------------------------------------------------- */

static void test_alert_hard_brake_critical(void)
{
    char buf[128];
    const char *want = "{\"node_id\":\"rw612-a\",\"ts_ms\":123789,\"seq\":17,\"alert_type\":\"hard_brake\","
                       "\"severity\":\"critical\"}";
    TEST_ASSERT_EQUAL_INT((int)strlen(want),
                          tlm_json_alert(buf, sizeof buf, NODE, 123789u, 17u, V2V_ALERT_HARD_BRAKE, V2V_SEVERITY_CRITICAL));
    TEST_ASSERT_EQUAL_STRING(want, buf);
}

static void test_alert_every_type_and_severity(void)
{
    char buf[128];
    TEST_ASSERT_TRUE(tlm_json_alert(buf, sizeof buf, NODE, 1u, 0u, V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"alert_type\":\"obstacle\",\"severity\":\"warning\""));
    TEST_ASSERT_TRUE(tlm_json_alert(buf, sizeof buf, NODE, 1u, 65535u, V2V_ALERT_OVERTEMP, V2V_SEVERITY_INFO) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"seq\":65535,\"alert_type\":\"overtemp\",\"severity\":\"info\""));
    TEST_ASSERT_TRUE(tlm_json_alert(buf, sizeof buf, NODE, 1u, 1u, V2V_ALERT_MANUAL, V2V_SEVERITY_CRITICAL) > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"alert_type\":\"manual\""));
}

static void test_alert_none_or_unknown_is_rejected(void)
{
    char buf[128] = "x";
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_alert(buf, sizeof buf, NODE, 1u, 1u, V2V_ALERT_NONE, V2V_SEVERITY_INFO));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_alert(buf, sizeof buf, NODE, 1u, 1u, (v2v_alert_type_t)9, V2V_SEVERITY_INFO));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_alert(buf, sizeof buf, NODE, 1u, 1u, V2V_ALERT_OBSTACLE, (v2v_severity_t)9));
}

/* ------------------------------------------------------------------------- */
/* status                                                                    */
/* ------------------------------------------------------------------------- */

static void test_status_online_offline(void)
{
    char buf[64];
    const char *on  = "{\"node_id\":\"rw612-a\",\"state\":\"online\"}";
    const char *off = "{\"node_id\":\"rw612-a\",\"state\":\"offline\"}";
    TEST_ASSERT_EQUAL_INT((int)strlen(on), tlm_json_status(buf, sizeof buf, NODE, true));
    TEST_ASSERT_EQUAL_STRING(on, buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(off), tlm_json_status(buf, sizeof buf, NODE, false));
    TEST_ASSERT_EQUAL_STRING(off, buf);
}

/* ------------------------------------------------------------------------- */
/* Safety: truncation, NULLs, node_id validation                              */
/* ------------------------------------------------------------------------- */

static void test_truncation_returns_minus_one_and_never_writes_past_n(void)
{
    /* Every n below strlen + 1 must fail without a write past n. */
    const size_t len = strlen("{\"node_id\":\"rw612-a\",\"state\":\"online\"}");
    for (size_t n = 0u; n <= (len + 1u); n++)
    {
        char guarded[64];
        memset(guarded, 0x5A, sizeof guarded);
        int r = tlm_json_status(guarded, n, NODE, true);
        if (n >= (len + 1u))
        {
            TEST_ASSERT_EQUAL_INT((int)len, r);
        }
        else
        {
            TEST_ASSERT_EQUAL_INT(-1, r);
            if (n > 0u)
            {
                TEST_ASSERT_EQUAL_CHAR('\0', guarded[0]); /* empty string, never a half payload */
            }
        }
        for (size_t i = n; i < sizeof guarded; i++)
        {
            TEST_ASSERT_EQUAL_HEX8(0x5A, (unsigned char)guarded[i]);
        }
    }
}

static void test_telemetry_truncation_never_writes_past_n(void)
{
    tlm_json_telemetry_t t = nominal();
    char big[256];
    int full = tlm_json_telemetry(big, sizeof big, &t);
    TEST_ASSERT_TRUE(full > 0);
    for (size_t n = 1u; n < (size_t)full + 1u; n++)
    {
        char guarded[256];
        memset(guarded, 0x5A, sizeof guarded);
        TEST_ASSERT_EQUAL_INT(-1, tlm_json_telemetry(guarded, n, &t));
        TEST_ASSERT_EQUAL_CHAR('\0', guarded[0]);
        for (size_t i = n; i < sizeof guarded; i++)
        {
            TEST_ASSERT_EQUAL_HEX8(0x5A, (unsigned char)guarded[i]);
        }
    }
}

static void test_null_arguments(void)
{
    char buf[64];
    tlm_json_telemetry_t t = nominal();
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_telemetry(NULL, 64u, &t));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_telemetry(buf, sizeof buf, NULL));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(NULL, 64u, NODE, true));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(buf, sizeof buf, NULL, true));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_alert(NULL, 64u, NODE, 1u, 1u, V2V_ALERT_OBSTACLE, V2V_SEVERITY_INFO));
}

static void test_node_id_must_match_schema_pattern(void)
{
    char buf[128];
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(buf, sizeof buf, "", true));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(buf, sizeof buf, "RW612", true));        /* uppercase */
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(buf, sizeof buf, "a\"b", true));         /* quote would break JSON */
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(buf, sizeof buf, "a b", true));
    TEST_ASSERT_EQUAL_INT(-1, tlm_json_status(buf, sizeof buf, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", true)); /* 33 */
    TEST_ASSERT_TRUE(tlm_json_status(buf, sizeof buf, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", true) > 0);       /* 32 */
    TEST_ASSERT_TRUE(tlm_json_status(buf, sizeof buf, "esp32-b", true) > 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_telemetry_nominal_without_optionals);
    RUN_TEST(test_telemetry_with_tps_and_sensor_ok);
    RUN_TEST(test_telemetry_null_on_sensor_fault);
    RUN_TEST(test_telemetry_nan_values_become_null_or_zero);
    RUN_TEST(test_telemetry_negative_and_rounding);
    RUN_TEST(test_telemetry_small_negative_has_no_minus_zero);
    RUN_TEST(test_telemetry_extreme_values_do_not_overflow);
    RUN_TEST(test_telemetry_fits_the_mqtt_ring_buffer);
    RUN_TEST(test_alert_hard_brake_critical);
    RUN_TEST(test_alert_every_type_and_severity);
    RUN_TEST(test_alert_none_or_unknown_is_rejected);
    RUN_TEST(test_status_online_offline);
    RUN_TEST(test_truncation_returns_minus_one_and_never_writes_past_n);
    RUN_TEST(test_telemetry_truncation_never_writes_past_n);
    RUN_TEST(test_null_arguments);
    RUN_TEST(test_node_id_must_match_schema_pattern);
    return UNITY_END();
}
