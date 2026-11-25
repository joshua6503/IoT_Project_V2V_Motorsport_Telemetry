/**
 * @file tlm_json.c
 * @brief JSON payload formatter. See tlm_json.h.
 */
#include "tlm_json.h"

#include <stddef.h>

#define NODE_ID_MAX_LEN 32u
#define FIXED1_MAX      99999999.0f /* clamp so tenths always fit a uint32 */

typedef struct
{
    char *buf;
    size_t n;
    size_t pos;
    bool ok;
} writer_t;

static void put_char(writer_t *w, char c)
{
    /* Keep one byte for the terminating NUL. */
    if (w->ok && ((w->pos + 1u) < w->n))
    {
        w->buf[w->pos++] = c;
    }
    else
    {
        w->ok = false;
    }
}

static void put_str(writer_t *w, const char *s)
{
    while (*s != '\0')
    {
        put_char(w, *s++);
    }
}

static void put_u32(writer_t *w, uint32_t v)
{
    char tmp[10];
    size_t k = 0u;
    do
    {
        tmp[k++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v != 0u);
    while (k > 0u)
    {
        put_char(w, tmp[--k]);
    }
}

static bool is_finite(float x)
{
    /* NaN != NaN; infinities fall outside +-FLT_MAX. No <math.h> needed. */
    return (x == x) && (x <= 3.4028234e38f) && (x >= -3.4028234e38f);
}

/** One decimal, round half away from zero, no "-0.0". */
static void put_fixed1(writer_t *w, float x)
{
    bool neg = (x < 0.0f);
    float a  = neg ? -x : x;
    if (a > FIXED1_MAX)
    {
        a = FIXED1_MAX;
    }
    uint32_t tenths = (uint32_t)((a * 10.0f) + 0.5f);
    if (neg && (tenths != 0u))
    {
        put_char(w, '-');
    }
    put_u32(w, tenths / 10u);
    put_char(w, '.');
    put_char(w, (char)('0' + (tenths % 10u)));
}

static bool node_id_valid(const char *id)
{
    if (id == NULL)
    {
        return false;
    }
    size_t len = 0u;
    for (; id[len] != '\0'; len++)
    {
        char c  = id[len];
        bool ok = ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '-');
        if (!ok || (len >= NODE_ID_MAX_LEN))
        {
            return false;
        }
    }
    return len > 0u;
}

static bool writer_begin(writer_t *w, char *buf, size_t n, const char *node_id)
{
    w->buf = buf;
    w->n   = n;
    w->pos = 0u;
    w->ok  = (buf != NULL) && (n > 0u);
    if (!w->ok)
    {
        return false;
    }
    buf[0] = '\0';
    if (!node_id_valid(node_id))
    {
        w->ok = false;
        return false;
    }
    put_str(w, "{\"node_id\":\"");
    put_str(w, node_id);
    put_char(w, '"');
    return w->ok;
}

static int writer_end(writer_t *w)
{
    put_char(w, '}');
    if (!w->ok)
    {
        if ((w->buf != NULL) && (w->n > 0u))
        {
            w->buf[0] = '\0';
        }
        return -1;
    }
    w->buf[w->pos] = '\0';
    return (int)w->pos;
}

static void put_u32_field(writer_t *w, const char *key, uint32_t v)
{
    put_str(w, ",\"");
    put_str(w, key);
    put_str(w, "\":");
    put_u32(w, v);
}

static void put_fixed1_field(writer_t *w, const char *key, float v)
{
    put_str(w, ",\"");
    put_str(w, key);
    put_str(w, "\":");
    put_fixed1(w, v);
}

static void put_opt_fixed1_field(writer_t *w, const char *key, bool valid, float v)
{
    if (valid && is_finite(v))
    {
        put_fixed1_field(w, key, v);
    }
    else
    {
        put_str(w, ",\"");
        put_str(w, key);
        put_str(w, "\":null");
    }
}

int tlm_json_telemetry(char *buf, size_t n, const tlm_json_telemetry_t *t)
{
    writer_t w;
    if (t == NULL)
    {
        if ((buf != NULL) && (n > 0u))
        {
            buf[0] = '\0';
        }
        return -1;
    }
    if (!writer_begin(&w, buf, n, t->node_id))
    {
        return writer_end(&w);
    }
    put_u32_field(&w, "ts_ms", t->ts_ms);
    put_u32_field(&w, "seq", t->seq);
    put_fixed1_field(&w, "brake_pct", is_finite(t->brake_pct) ? t->brake_pct : 0.0f);
    put_opt_fixed1_field(&w, "temp_c", t->temp_valid, t->temp_c);
    put_opt_fixed1_field(&w, "distance_cm", t->dist_valid, t->distance_cm);
    if (t->has_tps)
    {
        put_fixed1_field(&w, "tps_pct", is_finite(t->tps_pct) ? t->tps_pct : 0.0f);
    }
    if (t->has_sensor_ok)
    {
        put_u32_field(&w, "sensor_ok", t->sensor_ok);
    }
    return writer_end(&w);
}

int tlm_json_value1(char *buf, size_t n, bool valid, float x)
{
    writer_t w;
    w.buf = buf;
    w.n   = n;
    w.pos = 0u;
    w.ok  = (buf != NULL) && (n > 0u);
    if (!w.ok)
    {
        return -1;
    }
    buf[0] = '\0';
    if (valid && is_finite(x))
    {
        put_fixed1(&w, x);
    }
    else
    {
        put_str(&w, "null");
    }
    if (!w.ok)
    {
        buf[0] = '\0';
        return -1;
    }
    buf[w.pos] = '\0';
    return (int)w.pos;
}

static const char *alert_type_str(v2v_alert_type_t t)
{
    switch (t)
    {
        case V2V_ALERT_MANUAL:
            return "manual";
        case V2V_ALERT_HARD_BRAKE:
            return "hard_brake";
        case V2V_ALERT_OBSTACLE:
            return "obstacle";
        case V2V_ALERT_OVERTEMP:
            return "overtemp";
        default:
            return NULL; /* V2V_ALERT_NONE or unknown */
    }
}

static const char *severity_str(v2v_severity_t s)
{
    switch (s)
    {
        case V2V_SEVERITY_INFO:
            return "info";
        case V2V_SEVERITY_WARNING:
            return "warning";
        case V2V_SEVERITY_CRITICAL:
            return "critical";
        default:
            return NULL;
    }
}

int tlm_json_alert(char *buf, size_t n, const char *node_id, uint32_t ts_ms, uint16_t seq, v2v_alert_type_t type,
                   v2v_severity_t severity)
{
    writer_t w;
    const char *ts = alert_type_str(type);
    const char *ss = severity_str(severity);
    if ((ts == NULL) || (ss == NULL))
    {
        if ((buf != NULL) && (n > 0u))
        {
            buf[0] = '\0';
        }
        return -1;
    }
    if (!writer_begin(&w, buf, n, node_id))
    {
        return writer_end(&w);
    }
    put_u32_field(&w, "ts_ms", ts_ms);
    put_u32_field(&w, "seq", seq);
    put_str(&w, ",\"alert_type\":\"");
    put_str(&w, ts);
    put_str(&w, "\",\"severity\":\"");
    put_str(&w, ss);
    put_char(&w, '"');
    return writer_end(&w);
}

int tlm_json_status(char *buf, size_t n, const char *node_id, bool online)
{
    writer_t w;
    if (!writer_begin(&w, buf, n, node_id))
    {
        return writer_end(&w);
    }
    put_str(&w, ",\"state\":\"");
    put_str(&w, online ? "online" : "offline");
    put_char(&w, '"');
    return writer_end(&w);
}
