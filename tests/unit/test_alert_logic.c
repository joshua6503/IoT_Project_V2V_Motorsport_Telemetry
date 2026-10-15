/**
 * @file test_alert_logic.c
 * @brief Unity tests for firmware/common (alert_logic, alert_pattern) and the protocol header.
 */
#include <stddef.h>

#include "unity.h"

#include "alert_logic.h"
#include "alert_pattern.h"
#include "v2v_protocol.h"

static alert_ctx_t ctx;
static alert_config_t cfg;

void setUp(void)
{
    alert_config_default(&cfg);
    alert_init(&ctx, &cfg);
}

void tearDown(void) {}

/* Sample with every sensor valid and every reading well away from a threshold. */
static telemetry_t nominal_sample(uint32_t ts_ms)
{
    telemetry_t s = {0};
    s.ts_ms       = ts_ms;
    s.temp_c      = cfg.temp_off_c - 5.0f;
    s.distance_cm = cfg.obstacle_warn_cm * 10.0f;
    s.brake_pct   = 0.0f;
    s.tps_pct     = 0.0f;
    s.sensor_ok   = ALERT_SENSOR_REQUIRED;
    return s;
}

static alert_t eval(const telemetry_t *s)
{
    alert_t a;
    (void)alert_evaluate(&ctx, s, &a);
    return a;
}

/* Brake from 0 to cfg.brake_hard_pct in dt_ms; returns the alert of the second sample. */
static alert_t brake_press(uint32_t t0, uint32_t dt_ms)
{
    telemetry_t s = nominal_sample(t0);
    (void)eval(&s);
    s.ts_ms     = t0 + dt_ms;
    s.brake_pct = cfg.brake_hard_pct;
    return eval(&s);
}

static alert_t make_alert(v2v_alert_type_t type, v2v_severity_t sev, bool fault)
{
    alert_t a = {type, sev, fault};
    return a;
}

/* ------------------------------------------------------------------------- */
/* Protocol contract                                                         */
/* ------------------------------------------------------------------------- */

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

/* ------------------------------------------------------------------------- */
/* alert_evaluate: general                                                   */
/* ------------------------------------------------------------------------- */

static void test_alert_evaluate_rejects_null_arguments(void)
{
    telemetry_t s = nominal_sample(1000u);
    alert_t a;
    TEST_ASSERT_FALSE(alert_evaluate(NULL, &s, &a));
    TEST_ASSERT_FALSE(alert_evaluate(&ctx, NULL, &a));
    TEST_ASSERT_FALSE(alert_evaluate(&ctx, &s, NULL));
}

static void test_alert_evaluate_nominal_sample_raises_nothing(void)
{
    telemetry_t s = nominal_sample(1000u);
    alert_t a     = make_alert(V2V_ALERT_MANUAL, V2V_SEVERITY_CRITICAL, true);
    TEST_ASSERT_FALSE(alert_evaluate(&ctx, &s, &a));
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_INFO, a.severity);
    TEST_ASSERT_FALSE(a.sensor_fault);
}

static void test_alert_init_null_config_uses_defaults(void)
{
    alert_ctx_t c;
    alert_init(&c, NULL);
    TEST_ASSERT_EQUAL_FLOAT(cfg.obstacle_warn_cm, c.cfg.obstacle_warn_cm);
    TEST_ASSERT_EQUAL_UINT32(cfg.hard_brake_hold_ms, c.cfg.hard_brake_hold_ms);
}

static void test_alert_config_default_is_consistent(void)
{
    TEST_ASSERT_TRUE(cfg.obstacle_crit_cm < cfg.obstacle_warn_cm);
    TEST_ASSERT_TRUE(cfg.temp_off_c < cfg.temp_on_c);
    TEST_ASSERT_TRUE(cfg.temp_on_c < cfg.temp_crit_c);
    TEST_ASSERT_TRUE(cfg.hard_brake_hold_ms > 0u);
}

/* ------------------------------------------------------------------------- */
/* OBSTACLE                                                                  */
/* ------------------------------------------------------------------------- */

static void test_obstacle_below_warn_is_warning(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = cfg.obstacle_warn_cm - 1.0f;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OBSTACLE, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_WARNING, a.severity);
}

static void test_obstacle_at_warn_is_not_an_alert(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = cfg.obstacle_warn_cm;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, eval(&s).type);
}

static void test_obstacle_below_crit_is_critical(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = cfg.obstacle_crit_cm - 1.0f;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OBSTACLE, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_CRITICAL, a.severity);
}

static void test_obstacle_no_echo_is_fault_without_alert(void)
{
    /* The app reports "no echo" by clearing the DIST bit (DEC-008). */
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = -1.0f;
    s.sensor_ok   = ALERT_SENSOR_TEMP | ALERT_SENSOR_BRAKE;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
    TEST_ASSERT_TRUE(a.sensor_fault);
}

static void test_fault_comes_only_from_sensor_ok_bits(void)
{
    /* DEC-008: a negative distance with the DIST bit set raises no alert and no fault. */
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = -1.0f;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
    TEST_ASSERT_FALSE(a.sensor_fault);
}

static void test_obstacle_out_of_range_is_valid_no_alert(void)
{
    /* DEC-008: echo still high at the timeout -> HCSR04_MAX_CM (500 cm), valid. */
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = 500.0f;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
    TEST_ASSERT_FALSE(a.sensor_fault);
}

static void test_missing_sensor_bit_is_fault(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.sensor_ok   = ALERT_SENSOR_DIST | ALERT_SENSOR_BRAKE; /* temperature missing */
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
    TEST_ASSERT_TRUE(a.sensor_fault);
}

static void test_fault_does_not_suppress_valid_alert(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.sensor_ok   = ALERT_SENSOR_DIST | ALERT_SENSOR_BRAKE; /* temperature missing */
    s.distance_cm = cfg.obstacle_crit_cm - 1.0f;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OBSTACLE, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_CRITICAL, a.severity);
    TEST_ASSERT_TRUE(a.sensor_fault);
}

/* ------------------------------------------------------------------------- */
/* OVERTEMP                                                                  */
/* ------------------------------------------------------------------------- */

static void test_overtemp_turns_on_at_temp_on(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.temp_c      = cfg.temp_on_c;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OVERTEMP, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_WARNING, a.severity);
}

static void test_overtemp_band_without_latch_stays_off(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.temp_c      = (cfg.temp_on_c + cfg.temp_off_c) / 2.0f;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, eval(&s).type);
}

static void test_overtemp_hysteresis_holds_inside_band(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.temp_c      = cfg.temp_on_c;
    (void)eval(&s);
    s.ts_ms += 100u;
    s.temp_c = (cfg.temp_on_c + cfg.temp_off_c) / 2.0f;
    alert_t a = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OVERTEMP, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_WARNING, a.severity);
}

static void test_overtemp_releases_at_temp_off(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.temp_c      = cfg.temp_on_c;
    (void)eval(&s);
    s.ts_ms += 100u;
    s.temp_c = cfg.temp_off_c;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, eval(&s).type);
}

static void test_overtemp_critical_at_temp_crit(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.temp_c      = cfg.temp_crit_c;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OVERTEMP, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_CRITICAL, a.severity);
}

static void test_overtemp_invalid_reading_releases_latch(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.temp_c      = cfg.temp_on_c;
    (void)eval(&s);
    s.ts_ms += 100u;
    s.sensor_ok = ALERT_SENSOR_DIST | ALERT_SENSOR_BRAKE;
    alert_t a   = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, a.type);
    TEST_ASSERT_TRUE(a.sensor_fault);
}

/* ------------------------------------------------------------------------- */
/* HARD_BRAKE                                                                */
/* ------------------------------------------------------------------------- */

static void test_hard_brake_fast_press_triggers(void)
{
    /* 0 -> brake_hard_pct in 50 ms: rate far above brake_rate_pct_per_s. */
    alert_t a = brake_press(1000u, 50u);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_HARD_BRAKE, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_CRITICAL, a.severity);
}

static void test_hard_brake_slow_press_does_not_trigger(void)
{
    /* Same final level, reached in steps whose rate stays below the threshold. */
    float step_pct  = cfg.brake_rate_pct_per_s / 20.0f; /* per 100 ms = half the rate */
    telemetry_t s   = nominal_sample(1000u);
    alert_t a       = eval(&s);
    while (s.brake_pct < cfg.brake_hard_pct + step_pct)
    {
        s.ts_ms += 100u;
        s.brake_pct += step_pct;
        a = eval(&s);
        TEST_ASSERT_NOT_EQUAL_INT(V2V_ALERT_HARD_BRAKE, a.type);
    }
    TEST_ASSERT_TRUE(s.brake_pct >= cfg.brake_hard_pct);
}

static void test_hard_brake_fast_but_low_does_not_trigger(void)
{
    telemetry_t s = nominal_sample(1000u);
    (void)eval(&s);
    s.ts_ms += 50u;
    s.brake_pct = cfg.brake_hard_pct - 1.0f;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, eval(&s).type);
}

static void test_hard_brake_first_sample_has_no_rate(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.brake_pct   = 100.0f;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, eval(&s).type);
}

static void test_hard_brake_latch_holds_then_expires(void)
{
    uint32_t t_trigger = 1050u;
    (void)brake_press(1000u, 50u);

    telemetry_t s = nominal_sample(t_trigger + cfg.hard_brake_hold_ms - 1u); /* brake released */
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_HARD_BRAKE, eval(&s).type);

    s.ts_ms = t_trigger + cfg.hard_brake_hold_ms;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_NONE, eval(&s).type);
}

static void test_hard_brake_latch_survives_timer_wrap(void)
{
    uint32_t t0 = 0xFFFFFFFFu - 20u;
    alert_t a   = brake_press(t0, 50u); /* trigger lands after the wrap */
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_HARD_BRAKE, a.type);
    telemetry_t s = nominal_sample(t0 + 50u + cfg.hard_brake_hold_ms / 2u);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_HARD_BRAKE, eval(&s).type);
}

/* ------------------------------------------------------------------------- */
/* Arbitration (DEC-003)                                                     */
/* ------------------------------------------------------------------------- */

static void test_arbitration_highest_severity_wins(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = cfg.obstacle_warn_cm - 1.0f; /* OBSTACLE WARNING */
    s.temp_c      = cfg.temp_crit_c;             /* OVERTEMP CRITICAL */
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OVERTEMP, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_CRITICAL, a.severity);
}

static void test_arbitration_tie_hard_brake_over_obstacle(void)
{
    telemetry_t s = nominal_sample(1000u);
    (void)eval(&s);
    s.ts_ms += 50u;
    s.brake_pct   = cfg.brake_hard_pct;
    s.distance_cm = cfg.obstacle_crit_cm - 1.0f;
    s.temp_c      = cfg.temp_crit_c;
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_HARD_BRAKE, eval(&s).type);
}

static void test_arbitration_tie_obstacle_over_overtemp(void)
{
    telemetry_t s = nominal_sample(1000u);
    s.distance_cm = cfg.obstacle_warn_cm - 1.0f;
    s.temp_c      = cfg.temp_on_c;
    alert_t a     = eval(&s);
    TEST_ASSERT_EQUAL_INT(V2V_ALERT_OBSTACLE, a.type);
    TEST_ASSERT_EQUAL_INT(V2V_SEVERITY_WARNING, a.severity);
}

/* ------------------------------------------------------------------------- */
/* alert_pattern_for (DEC-004)                                               */
/* ------------------------------------------------------------------------- */

static void assert_pattern(alert_pattern_t p, bool g, bool y, bool r, bool bz)
{
    TEST_ASSERT_EQUAL(g, p.led_green);
    TEST_ASSERT_EQUAL(y, p.led_yellow);
    TEST_ASSERT_EQUAL(r, p.led_red);
    TEST_ASSERT_EQUAL(bz, p.buzzer_on);
}

static void test_pattern_none_is_green_only(void)
{
    alert_t a = make_alert(V2V_ALERT_NONE, V2V_SEVERITY_INFO, false);
    assert_pattern(alert_pattern_for(&a, 0u), true, false, false, false);
    assert_pattern(alert_pattern_for(&a, 777u), true, false, false, false);
}

static void test_pattern_null_is_green_only(void)
{
    assert_pattern(alert_pattern_for(NULL, 0u), true, false, false, false);
}

static void test_pattern_warning_yellow_and_1hz_beep(void)
{
    alert_t a = make_alert(V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING, false);
    assert_pattern(alert_pattern_for(&a, 0u), false, true, false, true);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_WARN_BEEP_MS - 1u), false, true, false, true);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_WARN_BEEP_MS), false, true, false, false);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_WARN_PERIOD_MS - 1u), false, true, false, false);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_WARN_PERIOD_MS), false, true, false, true);
}

static void test_pattern_critical_red_blink_4hz_with_buzzer(void)
{
    alert_t a = make_alert(V2V_ALERT_HARD_BRAKE, V2V_SEVERITY_CRITICAL, false);
    assert_pattern(alert_pattern_for(&a, 0u), false, false, true, true);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_CRIT_ON_MS - 1u), false, false, true, true);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_CRIT_ON_MS), false, false, false, false);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_CRIT_PERIOD_MS), false, false, true, true);
    TEST_ASSERT_EQUAL_UINT(4u, 1000u / ALERT_PATTERN_CRIT_PERIOD_MS);
}

static void test_pattern_sensor_fault_alternates_red_yellow_1hz(void)
{
    alert_t a = make_alert(V2V_ALERT_NONE, V2V_SEVERITY_INFO, true);
    assert_pattern(alert_pattern_for(&a, 0u), false, false, true, false);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_FAULT_PERIOD_MS / 2u - 1u), false, false, true, false);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_FAULT_PERIOD_MS / 2u), false, true, false, false);
    assert_pattern(alert_pattern_for(&a, ALERT_PATTERN_FAULT_PERIOD_MS), false, false, true, false);
}

static void test_pattern_alert_takes_precedence_over_fault(void)
{
    alert_t a = make_alert(V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING, true);
    assert_pattern(alert_pattern_for(&a, 0u), false, true, false, true);
}

/* ------------------------------------------------------------------------- */
/* alert_shift_level                                                         */
/* ------------------------------------------------------------------------- */

static void test_shift_level_bounds(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, alert_shift_level(-10.0f, 8u));
    TEST_ASSERT_EQUAL_UINT8(0u, alert_shift_level(0.0f, 8u));
    TEST_ASSERT_EQUAL_UINT8(8u, alert_shift_level(100.0f, 8u));
    TEST_ASSERT_EQUAL_UINT8(8u, alert_shift_level(150.0f, 8u));
}

static void test_shift_level_zero_leds(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, alert_shift_level(100.0f, 0u));
}

static void test_shift_level_midpoints(void)
{
    TEST_ASSERT_EQUAL_UINT8(4u, alert_shift_level(50.0f, 8u));
    TEST_ASSERT_EQUAL_UINT8(7u, alert_shift_level(99.0f, 8u));
    TEST_ASSERT_EQUAL_UINT8(1u, alert_shift_level(12.5f, 8u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_protocol_packet_size_matches_contract);
    RUN_TEST(test_protocol_field_offsets_match_ble_gatt_md);
    RUN_TEST(test_protocol_version_encoding);

    RUN_TEST(test_alert_evaluate_rejects_null_arguments);
    RUN_TEST(test_alert_evaluate_nominal_sample_raises_nothing);
    RUN_TEST(test_alert_init_null_config_uses_defaults);
    RUN_TEST(test_alert_config_default_is_consistent);

    RUN_TEST(test_obstacle_below_warn_is_warning);
    RUN_TEST(test_obstacle_at_warn_is_not_an_alert);
    RUN_TEST(test_obstacle_below_crit_is_critical);
    RUN_TEST(test_obstacle_no_echo_is_fault_without_alert);
    RUN_TEST(test_fault_comes_only_from_sensor_ok_bits);
    RUN_TEST(test_obstacle_out_of_range_is_valid_no_alert);
    RUN_TEST(test_missing_sensor_bit_is_fault);
    RUN_TEST(test_fault_does_not_suppress_valid_alert);

    RUN_TEST(test_overtemp_turns_on_at_temp_on);
    RUN_TEST(test_overtemp_band_without_latch_stays_off);
    RUN_TEST(test_overtemp_hysteresis_holds_inside_band);
    RUN_TEST(test_overtemp_releases_at_temp_off);
    RUN_TEST(test_overtemp_critical_at_temp_crit);
    RUN_TEST(test_overtemp_invalid_reading_releases_latch);

    RUN_TEST(test_hard_brake_fast_press_triggers);
    RUN_TEST(test_hard_brake_slow_press_does_not_trigger);
    RUN_TEST(test_hard_brake_fast_but_low_does_not_trigger);
    RUN_TEST(test_hard_brake_first_sample_has_no_rate);
    RUN_TEST(test_hard_brake_latch_holds_then_expires);
    RUN_TEST(test_hard_brake_latch_survives_timer_wrap);

    RUN_TEST(test_arbitration_highest_severity_wins);
    RUN_TEST(test_arbitration_tie_hard_brake_over_obstacle);
    RUN_TEST(test_arbitration_tie_obstacle_over_overtemp);

    RUN_TEST(test_pattern_none_is_green_only);
    RUN_TEST(test_pattern_null_is_green_only);
    RUN_TEST(test_pattern_warning_yellow_and_1hz_beep);
    RUN_TEST(test_pattern_critical_red_blink_4hz_with_buzzer);
    RUN_TEST(test_pattern_sensor_fault_alternates_red_yellow_1hz);
    RUN_TEST(test_pattern_alert_takes_precedence_over_fault);

    RUN_TEST(test_shift_level_bounds);
    RUN_TEST(test_shift_level_zero_leds);
    RUN_TEST(test_shift_level_midpoints);
    return UNITY_END();
}
