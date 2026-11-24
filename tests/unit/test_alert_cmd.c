/**
 * @file test_alert_cmd.c
 * @brief Unity tests for firmware/common alert_cmd: DEC-013 parser, (origin, seq) de-duplication and
 *        the remote-alert merge.
 */
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "alert_cmd.h"

void setUp(void) {}
void tearDown(void) {}

static alert_cmd_status_t parse(const char *s, alert_cmd_t *out)
{
    return alert_cmd_parse(s, strlen(s), out);
}

#define ALERT_FULL \
    "{\"cmd\":\"alert\",\"seq\":7,\"origin\":\"dashboard\",\"alert_type\":\"obstacle\"," \
    "\"severity\":\"critical\",\"duration_ms\":5000,\"source_node\":\"esp32-b\"}"

/* ------------------------------------------------------------------------- */
/* parser: accepted                                                          */
/* ------------------------------------------------------------------------- */

static void test_alert_full(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, parse(ALERT_FULL, &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ALERT, c.kind);
    TEST_ASSERT_EQUAL_UINT32(7u, c.seq);
    TEST_ASSERT_EQUAL_STRING("dashboard", c.origin);
    TEST_ASSERT_EQUAL(V2V_ALERT_OBSTACLE, c.alert_type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_CRITICAL, c.severity);
    TEST_ASSERT_EQUAL_UINT32(5000u, c.duration_ms);
    TEST_ASSERT_EQUAL_STRING("esp32-b", c.source_node);
}

static void test_alert_defaults_duration_and_no_source(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK,
                      parse("{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"dashboard\",\"alert_type\":\"manual\","
                            "\"severity\":\"info\"}",
                            &c));
    TEST_ASSERT_EQUAL_UINT32(3000u, c.duration_ms);
    TEST_ASSERT_EQUAL_STRING("", c.source_node);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, c.alert_type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_INFO, c.severity);
}

static void test_all_alert_types_and_severities(void)
{
    static const struct
    {
        const char *name;
        v2v_alert_type_t t;
    } types[] = {{"manual", V2V_ALERT_MANUAL},
                 {"hard_brake", V2V_ALERT_HARD_BRAKE},
                 {"obstacle", V2V_ALERT_OBSTACLE},
                 {"overtemp", V2V_ALERT_OVERTEMP}};
    static const struct
    {
        const char *name;
        v2v_severity_t s;
    } sevs[] = {{"info", V2V_SEVERITY_INFO}, {"warning", V2V_SEVERITY_WARNING}, {"critical", V2V_SEVERITY_CRITICAL}};
    for (unsigned i = 0; i < 4; i++)
    {
        for (unsigned j = 0; j < 3; j++)
        {
            char buf[160];
            alert_cmd_t c;
            (void)snprintf(buf, sizeof buf,
                           "{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"%s\",\"severity\":\"%s\"}",
                           types[i].name, sevs[j].name);
            TEST_ASSERT_EQUAL(ALERT_CMD_OK, parse(buf, &c));
            TEST_ASSERT_EQUAL(types[i].t, c.alert_type);
            TEST_ASSERT_EQUAL(sevs[j].s, c.severity);
        }
    }
}

static void test_clear(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, parse("{\"cmd\":\"clear\",\"seq\":8,\"origin\":\"dashboard\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_CLEAR, c.kind);
    TEST_ASSERT_EQUAL_UINT32(8u, c.seq);
    TEST_ASSERT_EQUAL_STRING("dashboard", c.origin);
}

static void test_clear_ignores_alert_fields_even_invalid(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK,
                      parse("{\"cmd\":\"clear\",\"seq\":8,\"origin\":\"d\",\"severity\":\"loud\",\"duration_ms\":1}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_CLEAR, c.kind);
}

static void test_whitespace_and_key_order(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK,
                      parse(" {\r\n \"origin\" : \"dashboard\" ,\t\"seq\" : 3 , \"severity\":\"warning\","
                            "\"alert_type\":\"overtemp\", \"cmd\":\"alert\" } \n",
                            &c));
    TEST_ASSERT_EQUAL_UINT32(3u, c.seq);
    TEST_ASSERT_EQUAL(V2V_ALERT_OVERTEMP, c.alert_type);
}

static void test_unknown_keys_are_ignored(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK,
                      parse("{\"cmd\":\"clear\",\"seq\":1,\"origin\":\"d\",\"extra\":\"x\",\"n\":-4.5e3,\"b\":true,"
                            "\"z\":null,\"a_very_long_unknown_key_name\":\"y\"}",
                            &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_CLEAR, c.kind);
}

static void test_escaped_slash_and_quote_in_unknown_value(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK,
                      parse("{\"cmd\":\"clear\",\"seq\":1,\"origin\":\"d\",\"note\":\"a\\\"b\\\\c\\/d\"}", &c));
}

static void test_output_zeroed_on_error(void)
{
    alert_cmd_t c;
    memset(&c, 0xAA, sizeof c);
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("nope", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_NONE, c.kind);
    TEST_ASSERT_EQUAL_STRING("", c.origin);
}

/* ------------------------------------------------------------------------- */
/* parser: seq                                                               */
/* ------------------------------------------------------------------------- */

static alert_cmd_status_t clear_with_seq(const char *seq_text, alert_cmd_t *c)
{
    char buf[96];
    (void)snprintf(buf, sizeof buf, "{\"cmd\":\"clear\",\"seq\":%s,\"origin\":\"d\"}", seq_text);
    return parse(buf, c);
}

static void test_seq_edges(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, clear_with_seq("0", &c));
    TEST_ASSERT_EQUAL_UINT32(0u, c.seq);
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, clear_with_seq("4294967295", &c));
    TEST_ASSERT_EQUAL_UINT32(4294967295u, c.seq);
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_seq("4294967296", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_seq("99999999999999999999", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_seq("-1", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_seq("1.5", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_seq("\"7\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_seq("true", &c));
}

/* ------------------------------------------------------------------------- */
/* parser: duration                                                          */
/* ------------------------------------------------------------------------- */

static alert_cmd_status_t alert_with_duration(const char *dur_text, alert_cmd_t *c)
{
    char buf[200];
    (void)snprintf(buf, sizeof buf,
                   "{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"manual\",\"severity\":\"info\","
                   "\"duration_ms\":%s}",
                   dur_text);
    return parse(buf, c);
}

static void test_duration_edges(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("99", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, alert_with_duration("100", &c));
    TEST_ASSERT_EQUAL_UINT32(100u, c.duration_ms);
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, alert_with_duration("10000", &c));
    TEST_ASSERT_EQUAL_UINT32(10000u, c.duration_ms);
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("10001", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("0", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("-500", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("1e3", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("\"3000\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, alert_with_duration("4294967296", &c));
}

/* ------------------------------------------------------------------------- */
/* parser: ids                                                               */
/* ------------------------------------------------------------------------- */

static alert_cmd_status_t clear_with_origin(const char *origin_json, alert_cmd_t *c)
{
    char buf[160];
    (void)snprintf(buf, sizeof buf, "{\"cmd\":\"clear\",\"seq\":1,\"origin\":%s}", origin_json);
    return parse(buf, c);
}

static void test_origin_rules(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, clear_with_origin("\"a\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, clear_with_origin("\"rw612-a-9\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, clear_with_origin("\"12345678901234567890123456789012\"", &c)); /* 32 */
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("\"123456789012345678901234567890123\"", &c)); /* 33 */
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("\"\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("\"Dashboard\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("\"dash_board\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("\"dash board\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("\"a\\/b\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, clear_with_origin("42", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE,
                      clear_with_origin("\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"", &c)); /* beyond buffer */
}

static void test_source_node_rules(void)
{
    alert_cmd_t c;
    char buf[200];
    (void)snprintf(buf, sizeof buf,
                   "{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"manual\",\"severity\":\"info\","
                   "\"source_node\":\"Esp_32\"}");
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, parse(buf, &c));
    (void)snprintf(buf, sizeof buf,
                   "{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"manual\",\"severity\":\"info\","
                   "\"source_node\":\"\"}");
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE, parse(buf, &c));
}

/* ------------------------------------------------------------------------- */
/* parser: rejected                                                          */
/* ------------------------------------------------------------------------- */

static void test_unknown_cmd(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_UNKNOWN_CMD, parse("{\"cmd\":\"reboot\",\"seq\":1,\"origin\":\"d\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_UNKNOWN_CMD, parse("{\"cmd\":\"ALERT\",\"seq\":1,\"origin\":\"d\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_UNKNOWN_CMD, parse("{\"cmd\":5,\"seq\":1,\"origin\":\"d\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_UNKNOWN_CMD, parse("{\"cmd\":\"actuator_test\"}", &c)); /* wins over missing seq */
}

static void test_missing_fields(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING, parse("{}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING, parse("{\"seq\":1,\"origin\":\"d\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING, parse("{\"cmd\":\"clear\",\"origin\":\"d\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING, parse("{\"cmd\":\"clear\",\"seq\":1}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING,
                      parse("{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"severity\":\"info\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING,
                      parse("{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"manual\"}", &c));
}

static void test_bad_enum_values(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE,
                      parse("{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"crash\","
                            "\"severity\":\"info\"}",
                            &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE,
                      parse("{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":\"manual\","
                            "\"severity\":\"CRITICAL\"}",
                            &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_RANGE,
                      parse("{\"cmd\":\"alert\",\"seq\":1,\"origin\":\"d\",\"alert_type\":2,\"severity\":\"info\"}", &c));
}

static void test_duplicate_known_key(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_DUP_KEY, parse("{\"cmd\":\"clear\",\"seq\":1,\"seq\":2,\"origin\":\"d\"}", &c));
}

static void test_broken_json(void)
{
    alert_cmd_t c;
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"alert\",", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("   ", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("[]", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("\"cmd\"", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\"\"clear\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{cmd:\"clear\"}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\",}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\"} x", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\"}{}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"cle", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"cl\\ear\"}", &c));          /* unsupported escape */
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"cl\x01\"}", &c));          /* control character */
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\",\"o\":{\"a\":1}}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\",\"o\":[1]}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\",\"o\":nul}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, parse("{\"cmd\":\"clear\",\"o\":abc}", &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, alert_cmd_parse(NULL, 5, &c));
}

static void test_null_out_pointer(void)
{
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, alert_cmd_parse("{}", 2, NULL));
}

static void test_too_long(void)
{
    char big[ALERT_CMD_MAX_LEN + 8u];
    alert_cmd_t c;
    memset(big, ' ', sizeof big);
    big[0]                     = '{';
    big[ALERT_CMD_MAX_LEN - 1] = '}'; /* last byte of the at-limit slice */
    big[ALERT_CMD_MAX_LEN]     = ' ';
    big[ALERT_CMD_MAX_LEN + 1] = '\0';
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_TOO_LONG, alert_cmd_parse(big, ALERT_CMD_MAX_LEN + 1u, &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_MISSING, alert_cmd_parse(big, ALERT_CMD_MAX_LEN, &c)); /* exactly at the limit */
}

static void test_len_is_honoured_without_nul(void)
{
    alert_cmd_t c;
    const char *s = "{\"cmd\":\"clear\",\"seq\":1,\"origin\":\"d\"}GARBAGE";
    TEST_ASSERT_EQUAL(ALERT_CMD_OK, alert_cmd_parse(s, strlen(s) - 7u, &c));
    TEST_ASSERT_EQUAL(ALERT_CMD_ERR_JSON, alert_cmd_parse(s, strlen(s), &c));
}

static void test_status_names(void)
{
    TEST_ASSERT_EQUAL_STRING("OK", alert_cmd_status_name(ALERT_CMD_OK));
    TEST_ASSERT_EQUAL_STRING("ERR_JSON", alert_cmd_status_name(ALERT_CMD_ERR_JSON));
    TEST_ASSERT_EQUAL_STRING("ERR_TOO_LONG", alert_cmd_status_name(ALERT_CMD_ERR_TOO_LONG));
    TEST_ASSERT_EQUAL_STRING("ERR_UNKNOWN_CMD", alert_cmd_status_name(ALERT_CMD_ERR_UNKNOWN_CMD));
    TEST_ASSERT_EQUAL_STRING("ERR_MISSING", alert_cmd_status_name(ALERT_CMD_ERR_MISSING));
    TEST_ASSERT_EQUAL_STRING("ERR_RANGE", alert_cmd_status_name(ALERT_CMD_ERR_RANGE));
    TEST_ASSERT_EQUAL_STRING("ERR_DUP_KEY", alert_cmd_status_name(ALERT_CMD_ERR_DUP_KEY));
}

/* ------------------------------------------------------------------------- */
/* de-duplication                                                            */
/* ------------------------------------------------------------------------- */

static alert_cmd_t cmd_of(const char *origin, uint32_t seq)
{
    alert_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind = ALERT_CMD_CLEAR;
    c.seq  = seq;
    (void)strcpy(c.origin, origin);
    return c;
}

static void test_dedupe_same_origin_and_seq(void)
{
    alert_cmd_dedupe_t d;
    alert_cmd_t a = cmd_of("dashboard", 5);
    alert_cmd_dedupe_init(&d);
    TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &a));
    TEST_ASSERT_TRUE(alert_cmd_dedupe_seen(&d, &a));
    TEST_ASSERT_TRUE(alert_cmd_dedupe_seen(&d, &a));
}

static void test_dedupe_distinguishes_origin_and_seq(void)
{
    alert_cmd_dedupe_t d;
    alert_cmd_t a = cmd_of("dashboard", 5);
    alert_cmd_t b = cmd_of("dashboard", 6);
    alert_cmd_t c = cmd_of("tablet", 5);
    alert_cmd_dedupe_init(&d);
    TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &a));
    TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &b));
    TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &c));
    TEST_ASSERT_TRUE(alert_cmd_dedupe_seen(&d, &b));
}

static void test_dedupe_seq_zero_is_a_normal_seq(void)
{
    alert_cmd_dedupe_t d;
    alert_cmd_t a = cmd_of("dashboard", 0);
    alert_cmd_dedupe_init(&d);
    TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &a)); /* not confused with an empty slot */
    TEST_ASSERT_TRUE(alert_cmd_dedupe_seen(&d, &a));
}

static void test_dedupe_evicts_oldest_after_window(void)
{
    alert_cmd_dedupe_t d;
    alert_cmd_dedupe_init(&d);
    for (uint32_t i = 0; i < ALERT_CMD_DEDUPE_SLOTS; i++)
    {
        alert_cmd_t c = cmd_of("dashboard", i);
        TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &c));
    }
    {
        alert_cmd_t c = cmd_of("dashboard", 15);
        TEST_ASSERT_TRUE(alert_cmd_dedupe_seen(&d, &c)); /* still inside the window */
    }
    {
        alert_cmd_t c = cmd_of("dashboard", ALERT_CMD_DEDUPE_SLOTS); /* 17th distinct: evicts seq 0 */
        TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &c));
    }
    {
        alert_cmd_t c = cmd_of("dashboard", 0);
        TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(&d, &c)); /* forgotten, accepted again */
    }
}

static void test_dedupe_null_safe(void)
{
    alert_cmd_t a = cmd_of("d", 1);
    TEST_ASSERT_FALSE(alert_cmd_dedupe_seen(NULL, &a));
    alert_cmd_dedupe_init(NULL);
}

/* ------------------------------------------------------------------------- */
/* remote alert + merge                                                      */
/* ------------------------------------------------------------------------- */

static alert_cmd_t alert_cmd_of(v2v_alert_type_t t, v2v_severity_t s, uint32_t dur)
{
    alert_cmd_t c;
    memset(&c, 0, sizeof c);
    c.kind        = ALERT_CMD_ALERT;
    c.alert_type  = t;
    c.severity    = s;
    c.duration_ms = dur;
    (void)strcpy(c.origin, "dashboard");
    return c;
}

static const alert_t LOCAL_NONE = {V2V_ALERT_NONE, V2V_SEVERITY_INFO, false};

static void test_remote_inactive_passes_local_through(void)
{
    alert_remote_t r;
    alert_t local = {V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING, true};
    alert_remote_init(&r);
    alert_t m = alert_remote_merge(&r, &local, 1000);
    TEST_ASSERT_EQUAL(V2V_ALERT_OBSTACLE, m.type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_WARNING, m.severity);
    TEST_ASSERT_TRUE(m.sensor_fault);
}

static void test_remote_over_none_for_duration_then_expires(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_WARNING, 3000);
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 10000);
    alert_t m = alert_remote_merge(&r, &LOCAL_NONE, 10000);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, m.type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_WARNING, m.severity);
    m = alert_remote_merge(&r, &LOCAL_NONE, 12999);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, m.type);
    m = alert_remote_merge(&r, &LOCAL_NONE, 13000);
    TEST_ASSERT_EQUAL(V2V_ALERT_NONE, m.type);
    TEST_ASSERT_FALSE(r.active);
}

static void test_local_higher_severity_wins(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_WARNING, 3000);
    alert_t local = {V2V_ALERT_HARD_BRAKE, V2V_SEVERITY_CRITICAL, false};
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 0);
    alert_t m = alert_remote_merge(&r, &local, 100);
    TEST_ASSERT_EQUAL(V2V_ALERT_HARD_BRAKE, m.type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_CRITICAL, m.severity);
    TEST_ASSERT_TRUE(r.active); /* still pending: shows again once the local alert ends */
    m = alert_remote_merge(&r, &LOCAL_NONE, 200);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, m.type);
}

static void test_equal_severity_keeps_local(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_OVERTEMP, V2V_SEVERITY_WARNING, 3000);
    alert_t local = {V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING, false};
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 0);
    alert_t m = alert_remote_merge(&r, &local, 100);
    TEST_ASSERT_EQUAL(V2V_ALERT_OBSTACLE, m.type);
}

static void test_remote_higher_overrides_local(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_CRITICAL, 3000);
    alert_t local = {V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING, true};
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 0);
    alert_t m = alert_remote_merge(&r, &local, 100);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, m.type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_CRITICAL, m.severity);
    TEST_ASSERT_TRUE(m.sensor_fault); /* fault flag always from local */
}

static void test_clear_ends_early(void)
{
    alert_remote_t r;
    alert_cmd_t a  = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_CRITICAL, 10000);
    alert_cmd_t cl = cmd_of("dashboard", 2);
    alert_remote_init(&r);
    alert_remote_apply(&r, &a, 0);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, alert_remote_merge(&r, &LOCAL_NONE, 1000).type);
    alert_remote_apply(&r, &cl, 1500);
    TEST_ASSERT_EQUAL(V2V_ALERT_NONE, alert_remote_merge(&r, &LOCAL_NONE, 1501).type);
}

static void test_new_alert_replaces_previous(void)
{
    alert_remote_t r;
    alert_cmd_t a = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_WARNING, 1000);
    alert_cmd_t b = alert_cmd_of(V2V_ALERT_OVERTEMP, V2V_SEVERITY_CRITICAL, 5000);
    alert_remote_init(&r);
    alert_remote_apply(&r, &a, 0);
    alert_remote_apply(&r, &b, 500);
    alert_t m = alert_remote_merge(&r, &LOCAL_NONE, 3000); /* a would have ended at 1000 */
    TEST_ASSERT_EQUAL(V2V_ALERT_OVERTEMP, m.type);
    TEST_ASSERT_EQUAL(V2V_SEVERITY_CRITICAL, m.severity);
}

static void test_duration_capped_at_10_s(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_WARNING, 60000); /* bypasses the parser */
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 0);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, alert_remote_merge(&r, &LOCAL_NONE, 9999).type);
    TEST_ASSERT_EQUAL(V2V_ALERT_NONE, alert_remote_merge(&r, &LOCAL_NONE, 10000).type);
}

static void test_expiry_is_wrap_safe(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_WARNING, 3000);
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 0xFFFFFC00u); /* ends 2 ms... after the 32-bit wrap */
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, alert_remote_merge(&r, &LOCAL_NONE, 0xFFFFFFFFu).type);
    TEST_ASSERT_EQUAL(V2V_ALERT_MANUAL, alert_remote_merge(&r, &LOCAL_NONE, 1000u).type);
    TEST_ASSERT_EQUAL(V2V_ALERT_NONE, alert_remote_merge(&r, &LOCAL_NONE, 2000u).type);
}

static void test_info_remote_adds_nothing_visible(void)
{
    alert_remote_t r;
    alert_cmd_t c = alert_cmd_of(V2V_ALERT_MANUAL, V2V_SEVERITY_INFO, 3000);
    alert_t local = {V2V_ALERT_OBSTACLE, V2V_SEVERITY_WARNING, false};
    alert_remote_init(&r);
    alert_remote_apply(&r, &c, 0);
    alert_t m = alert_remote_merge(&r, &local, 100);
    TEST_ASSERT_EQUAL(V2V_ALERT_OBSTACLE, m.type); /* info never beats a local alert */
}

static void test_merge_null_safe(void)
{
    alert_t m = alert_remote_merge(NULL, NULL, 0);
    TEST_ASSERT_EQUAL(V2V_ALERT_NONE, m.type);
    alert_remote_apply(NULL, NULL, 0);
    alert_remote_init(NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_alert_full);
    RUN_TEST(test_alert_defaults_duration_and_no_source);
    RUN_TEST(test_all_alert_types_and_severities);
    RUN_TEST(test_clear);
    RUN_TEST(test_clear_ignores_alert_fields_even_invalid);
    RUN_TEST(test_whitespace_and_key_order);
    RUN_TEST(test_unknown_keys_are_ignored);
    RUN_TEST(test_escaped_slash_and_quote_in_unknown_value);
    RUN_TEST(test_output_zeroed_on_error);
    RUN_TEST(test_seq_edges);
    RUN_TEST(test_duration_edges);
    RUN_TEST(test_origin_rules);
    RUN_TEST(test_source_node_rules);
    RUN_TEST(test_unknown_cmd);
    RUN_TEST(test_missing_fields);
    RUN_TEST(test_bad_enum_values);
    RUN_TEST(test_duplicate_known_key);
    RUN_TEST(test_broken_json);
    RUN_TEST(test_null_out_pointer);
    RUN_TEST(test_too_long);
    RUN_TEST(test_len_is_honoured_without_nul);
    RUN_TEST(test_status_names);
    RUN_TEST(test_dedupe_same_origin_and_seq);
    RUN_TEST(test_dedupe_distinguishes_origin_and_seq);
    RUN_TEST(test_dedupe_seq_zero_is_a_normal_seq);
    RUN_TEST(test_dedupe_evicts_oldest_after_window);
    RUN_TEST(test_dedupe_null_safe);
    RUN_TEST(test_remote_inactive_passes_local_through);
    RUN_TEST(test_remote_over_none_for_duration_then_expires);
    RUN_TEST(test_local_higher_severity_wins);
    RUN_TEST(test_equal_severity_keeps_local);
    RUN_TEST(test_remote_higher_overrides_local);
    RUN_TEST(test_clear_ends_early);
    RUN_TEST(test_new_alert_replaces_previous);
    RUN_TEST(test_duration_capped_at_10_s);
    RUN_TEST(test_expiry_is_wrap_safe);
    RUN_TEST(test_info_remote_adds_nothing_visible);
    RUN_TEST(test_merge_null_safe);
    return UNITY_END();
}
