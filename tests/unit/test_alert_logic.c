/**
 * @file test_alert_logic.c
 * @brief Unity tests for firmware/common/alert_logic and the protocol header.
 */
#include <stddef.h>

#include "unity.h"

#include "alert_logic.h"
#include "v2v_protocol.h"

void setUp(void) {}
void tearDown(void) {}

static telemetry_t nominal_sample(void)
{
    telemetry_t s = {0};
    s.ts_ms       = 1000u;
    s.speed_kmh   = 30.0f;
    s.accel_g_z   = 1.0f;
    s.temp_c      = 25.0f;
    s.distance_cm = 300.0f;
    return s;
}

static void test_protocol_packet_size_matches_contract(void)
{
    TEST_ASSERT_EQUAL_UINT(V2V_PACKET_SIZE, sizeof(v2v_packet_t));
    TEST_ASSERT_EQUAL_UINT(V2V_NODE_INFO_SIZE, sizeof(v2v_node_info_t));
}

static void test_protocol_field_offsets_match_ble_gatt_md(void)
{
    TEST_ASSERT_EQUAL_UINT(0, offsetof(v2v_packet_t, version));
    TEST_ASSERT_EQUAL_UINT(1, offsetof(v2v_packet_t, alert_type));
    TEST_ASSERT_EQUAL_UINT(2, offsetof(v2v_packet_t, severity));
    TEST_ASSERT_EQUAL_UINT(3, offsetof(v2v_packet_t, flags));
    TEST_ASSERT_EQUAL_UINT(4, offsetof(v2v_packet_t, node_id));
    TEST_ASSERT_EQUAL_UINT(6, offsetof(v2v_packet_t, seq));
    TEST_ASSERT_EQUAL_UINT(8, offsetof(v2v_packet_t, timestamp_ms));
    TEST_ASSERT_EQUAL_UINT(12, offsetof(v2v_packet_t, crc16));
    TEST_ASSERT_EQUAL_UINT(14, offsetof(v2v_packet_t, reserved));
}

static void test_protocol_version_encoding(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x01, V2V_PROTOCOL_VERSION);
}

static void test_alert_evaluate_rejects_null_arguments(void)
{
    telemetry_t s = nominal_sample();
    alert_t a;
    TEST_ASSERT_FALSE(alert_evaluate(NULL, &a));
    TEST_ASSERT_FALSE(alert_evaluate(&s, NULL));
}

static void test_alert_evaluate_nominal_sample_raises_nothing(void)
{
    telemetry_t s = nominal_sample();
    alert_t a     = {V2V_ALERT_MANUAL, V2V_SEVERITY_CRITICAL};
    TEST_ASSERT_FALSE(alert_evaluate(&s, &a));
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_protocol_packet_size_matches_contract);
    RUN_TEST(test_protocol_field_offsets_match_ble_gatt_md);
    RUN_TEST(test_protocol_version_encoding);
    RUN_TEST(test_alert_evaluate_rejects_null_arguments);
    RUN_TEST(test_alert_evaluate_nominal_sample_raises_nothing);
    return UNITY_END();
}
