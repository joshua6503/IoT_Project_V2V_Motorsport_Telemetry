/**
 * @file alert_cmd.c
 * @brief Command parser, de-duplication and remote-alert merge. See alert_cmd.h.
 */
#include "alert_cmd.h"

#include <string.h>

#define KEY_MAX   16u /* longest known key is "duration_ms" (11) + NUL; longer keys are unknown */
#define VALUE_MAX 40u /* longest known value is a 32-char id; longer strings are truncated and rejected */

typedef struct
{
    const char *p;
    const char *end;
} cur_t;

typedef enum
{
    V_STR,
    V_UINT, /* only digits */
    V_OTHER /* true / false / null / signed or fractional number */
} vkind_t;

typedef struct
{
    vkind_t kind;
    char str[VALUE_MAX];
    bool trunc;
    uint32_t u;
    bool u_overflow;
} value_t;

static void skip_ws(cur_t *c)
{
    while ((c->p < c->end) && ((*c->p == ' ') || (*c->p == '\t') || (*c->p == '\r') || (*c->p == '\n')))
    {
        c->p++;
    }
}

/* c->p is at the opening quote. Only \" \\ \/ escapes; control characters are rejected. */
static bool scan_string(cur_t *c, char *out, size_t outsz, bool *trunc)
{
    size_t n = 0u;
    *trunc   = false;
    if ((c->p >= c->end) || (*c->p != '"'))
    {
        return false;
    }
    c->p++;
    while (c->p < c->end)
    {
        unsigned char ch = (unsigned char)*c->p++;
        if (ch == (unsigned char)'"')
        {
            out[n] = '\0';
            return true;
        }
        if (ch < 0x20u)
        {
            return false;
        }
        if (ch == (unsigned char)'\\')
        {
            if (c->p >= c->end)
            {
                return false;
            }
            ch = (unsigned char)*c->p++;
            if ((ch != (unsigned char)'"') && (ch != (unsigned char)'\\') && (ch != (unsigned char)'/'))
            {
                return false;
            }
        }
        if ((n + 1u) < outsz)
        {
            out[n++] = (char)ch;
        }
        else
        {
            *trunc = true;
        }
    }
    return false;
}

static bool scan_value(cur_t *c, value_t *v)
{
    memset(v, 0, sizeof *v);
    if (c->p >= c->end)
    {
        return false;
    }
    if (*c->p == '"')
    {
        v->kind = V_STR;
        return scan_string(c, v->str, sizeof v->str, &v->trunc);
    }
    if ((*c->p == '{') || (*c->p == '['))
    {
        return false; /* flat objects only */
    }

    const char *s = c->p;
    while ((c->p < c->end) && (*c->p != ',') && (*c->p != '}') && (*c->p != ' ') && (*c->p != '\t') &&
           (*c->p != '\r') && (*c->p != '\n'))
    {
        c->p++;
    }
    size_t n = (size_t)(c->p - s);
    if (n == 0u)
    {
        return false;
    }
    if (((n == 4u) && (memcmp(s, "true", 4) == 0)) || ((n == 5u) && (memcmp(s, "false", 5) == 0)) ||
        ((n == 4u) && (memcmp(s, "null", 4) == 0)))
    {
        v->kind = V_OTHER;
        return true;
    }
    bool digits = true;
    for (size_t i = 0u; i < n; i++)
    {
        char ch = s[i];
        if ((ch >= '0') && (ch <= '9'))
        {
            continue;
        }
        digits = false;
        if ((ch != '+') && (ch != '-') && (ch != '.') && (ch != 'e') && (ch != 'E'))
        {
            return false; /* not a JSON number or literal */
        }
    }
    if (!digits)
    {
        v->kind = V_OTHER;
        return true;
    }
    v->kind = V_UINT;
    for (size_t i = 0u; i < n; i++)
    {
        uint32_t d = (uint32_t)(s[i] - '0');
        if ((v->u > (UINT32_MAX / 10u)) || ((v->u * 10u) > (UINT32_MAX - d)))
        {
            v->u_overflow = true;
            break;
        }
        v->u = (v->u * 10u) + d;
    }
    return true;
}

static bool id_valid(const value_t *v)
{
    if ((v->kind != V_STR) || v->trunc)
    {
        return false;
    }
    size_t n = strlen(v->str);
    if ((n < 1u) || (n > ALERT_CMD_ID_MAX_LEN))
    {
        return false;
    }
    for (size_t i = 0u; i < n; i++)
    {
        char ch = v->str[i];
        if (!(((ch >= 'a') && (ch <= 'z')) || ((ch >= '0') && (ch <= '9')) || (ch == '-')))
        {
            return false;
        }
    }
    return true;
}

enum
{
    F_CMD    = 1u << 0,
    F_SEQ    = 1u << 1,
    F_ORIGIN = 1u << 2,
    F_TYPE   = 1u << 3,
    F_SEV    = 1u << 4,
    F_DUR    = 1u << 5,
    F_SOURCE = 1u << 6
};

typedef struct
{
    unsigned seen;
    unsigned bad; /* same bits: field present but invalid */
    alert_cmd_kind_t kind;
    bool unknown_cmd;
} fields_t;

static unsigned key_flag(const char *key)
{
    if (strcmp(key, "cmd") == 0)
    {
        return F_CMD;
    }
    if (strcmp(key, "seq") == 0)
    {
        return F_SEQ;
    }
    if (strcmp(key, "origin") == 0)
    {
        return F_ORIGIN;
    }
    if (strcmp(key, "alert_type") == 0)
    {
        return F_TYPE;
    }
    if (strcmp(key, "severity") == 0)
    {
        return F_SEV;
    }
    if (strcmp(key, "duration_ms") == 0)
    {
        return F_DUR;
    }
    if (strcmp(key, "source_node") == 0)
    {
        return F_SOURCE;
    }
    return 0u;
}

static void store_field(unsigned flag, const value_t *v, fields_t *f, alert_cmd_t *out)
{
    switch (flag)
    {
        case F_CMD:
            if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "alert") == 0))
            {
                f->kind = ALERT_CMD_ALERT;
            }
            else if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "clear") == 0))
            {
                f->kind = ALERT_CMD_CLEAR;
            }
            else
            {
                f->unknown_cmd = true;
            }
            break;
        case F_SEQ:
            if ((v->kind == V_UINT) && !v->u_overflow)
            {
                out->seq = v->u;
            }
            else
            {
                f->bad |= F_SEQ;
            }
            break;
        case F_ORIGIN:
            if (id_valid(v))
            {
                (void)strcpy(out->origin, v->str);
            }
            else
            {
                f->bad |= F_ORIGIN;
            }
            break;
        case F_TYPE:
            if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "manual") == 0))
            {
                out->alert_type = V2V_ALERT_MANUAL;
            }
            else if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "hard_brake") == 0))
            {
                out->alert_type = V2V_ALERT_HARD_BRAKE;
            }
            else if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "obstacle") == 0))
            {
                out->alert_type = V2V_ALERT_OBSTACLE;
            }
            else if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "overtemp") == 0))
            {
                out->alert_type = V2V_ALERT_OVERTEMP;
            }
            else
            {
                f->bad |= F_TYPE;
            }
            break;
        case F_SEV:
            if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "info") == 0))
            {
                out->severity = V2V_SEVERITY_INFO;
            }
            else if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "warning") == 0))
            {
                out->severity = V2V_SEVERITY_WARNING;
            }
            else if ((v->kind == V_STR) && !v->trunc && (strcmp(v->str, "critical") == 0))
            {
                out->severity = V2V_SEVERITY_CRITICAL;
            }
            else
            {
                f->bad |= F_SEV;
            }
            break;
        case F_DUR:
            if ((v->kind == V_UINT) && !v->u_overflow && (v->u >= ALERT_CMD_DURATION_MIN_MS) &&
                (v->u <= ALERT_CMD_DURATION_MAX_MS))
            {
                out->duration_ms = v->u;
            }
            else
            {
                f->bad |= F_DUR;
            }
            break;
        case F_SOURCE:
            if (id_valid(v))
            {
                (void)strcpy(out->source_node, v->str);
            }
            else
            {
                f->bad |= F_SOURCE;
            }
            break;
        default:
            break;
    }
}

const char *alert_cmd_status_name(alert_cmd_status_t s)
{
    switch (s)
    {
        case ALERT_CMD_OK:
            return "OK";
        case ALERT_CMD_ERR_JSON:
            return "ERR_JSON";
        case ALERT_CMD_ERR_TOO_LONG:
            return "ERR_TOO_LONG";
        case ALERT_CMD_ERR_UNKNOWN_CMD:
            return "ERR_UNKNOWN_CMD";
        case ALERT_CMD_ERR_MISSING:
            return "ERR_MISSING";
        case ALERT_CMD_ERR_RANGE:
            return "ERR_RANGE";
        case ALERT_CMD_ERR_DUP_KEY:
            return "ERR_DUP_KEY";
        default:
            return "ERR_?";
    }
}

alert_cmd_status_t alert_cmd_parse(const char *buf, size_t len, alert_cmd_t *out)
{
    fields_t f;
    cur_t c;
    char key[KEY_MAX];
    value_t v;
    bool trunc;

    if (out == NULL)
    {
        return ALERT_CMD_ERR_JSON;
    }
    memset(out, 0, sizeof *out);
    memset(&f, 0, sizeof f);
    out->duration_ms = ALERT_CMD_DURATION_DEFAULT_MS;
    out->alert_type  = V2V_ALERT_NONE;
    out->severity    = V2V_SEVERITY_INFO;
    if ((buf == NULL) || (len == 0u))
    {
        return ALERT_CMD_ERR_JSON;
    }
    if (len > ALERT_CMD_MAX_LEN)
    {
        return ALERT_CMD_ERR_TOO_LONG;
    }

    c.p   = buf;
    c.end = buf + len;
    skip_ws(&c);
    if ((c.p >= c.end) || (*c.p != '{'))
    {
        return ALERT_CMD_ERR_JSON;
    }
    c.p++;
    skip_ws(&c);
    if ((c.p < c.end) && (*c.p == '}'))
    {
        c.p++;
    }
    else
    {
        for (;;)
        {
            skip_ws(&c);
            if (!scan_string(&c, key, sizeof key, &trunc))
            {
                return ALERT_CMD_ERR_JSON;
            }
            skip_ws(&c);
            if ((c.p >= c.end) || (*c.p != ':'))
            {
                return ALERT_CMD_ERR_JSON;
            }
            c.p++;
            skip_ws(&c);
            if (!scan_value(&c, &v))
            {
                return ALERT_CMD_ERR_JSON;
            }

            unsigned flag = trunc ? 0u : key_flag(key);
            if (flag != 0u)
            {
                if ((f.seen & flag) != 0u)
                {
                    return ALERT_CMD_ERR_DUP_KEY;
                }
                f.seen |= flag;
                store_field(flag, &v, &f, out);
            }

            skip_ws(&c);
            if ((c.p < c.end) && (*c.p == ','))
            {
                c.p++;
                continue;
            }
            if ((c.p < c.end) && (*c.p == '}'))
            {
                c.p++;
                break;
            }
            return ALERT_CMD_ERR_JSON;
        }
    }
    skip_ws(&c);
    if (c.p != c.end)
    {
        return ALERT_CMD_ERR_JSON;
    }

    /* Syntax is fine; now the semantic checks. */
    if ((f.seen & F_CMD) == 0u)
    {
        return ALERT_CMD_ERR_MISSING;
    }
    if (f.unknown_cmd)
    {
        return ALERT_CMD_ERR_UNKNOWN_CMD;
    }
    if (((f.seen & F_SEQ) == 0u) || ((f.seen & F_ORIGIN) == 0u))
    {
        return ALERT_CMD_ERR_MISSING;
    }
    if ((f.bad & (F_SEQ | F_ORIGIN)) != 0u)
    {
        return ALERT_CMD_ERR_RANGE;
    }
    if (f.kind == ALERT_CMD_ALERT)
    {
        if (((f.seen & F_TYPE) == 0u) || ((f.seen & F_SEV) == 0u))
        {
            return ALERT_CMD_ERR_MISSING;
        }
        if ((f.bad & (F_TYPE | F_SEV | F_DUR | F_SOURCE)) != 0u)
        {
            return ALERT_CMD_ERR_RANGE;
        }
    }
    else
    {
        /* clear carries no alert fields: whatever was sent for them is ignored */
        out->alert_type     = V2V_ALERT_NONE;
        out->severity       = V2V_SEVERITY_INFO;
        out->duration_ms    = 0u;
        out->source_node[0] = '\0';
    }
    out->kind = f.kind;
    return ALERT_CMD_OK;
}

/* ------------------------------------------------------------------------- */

void alert_cmd_dedupe_init(alert_cmd_dedupe_t *d)
{
    if (d != NULL)
    {
        memset(d, 0, sizeof *d);
    }
}

bool alert_cmd_dedupe_seen(alert_cmd_dedupe_t *d, const alert_cmd_t *cmd)
{
    if ((d == NULL) || (cmd == NULL))
    {
        return false;
    }
    for (uint8_t i = 0u; i < ALERT_CMD_DEDUPE_SLOTS; i++)
    {
        if (d->used[i] && (d->seq[i] == cmd->seq) && (strcmp(d->origin[i], cmd->origin) == 0))
        {
            return true;
        }
    }
    (void)strcpy(d->origin[d->next], cmd->origin);
    d->seq[d->next]  = cmd->seq;
    d->used[d->next] = true;
    d->next          = (uint8_t)((d->next + 1u) % ALERT_CMD_DEDUPE_SLOTS);
    return false;
}

/* ------------------------------------------------------------------------- */

void alert_remote_init(alert_remote_t *r)
{
    if (r != NULL)
    {
        memset(r, 0, sizeof *r);
    }
}

void alert_remote_apply(alert_remote_t *r, const alert_cmd_t *cmd, uint32_t now_ms)
{
    if ((r == NULL) || (cmd == NULL))
    {
        return;
    }
    if (cmd->kind == ALERT_CMD_ALERT)
    {
        uint32_t d = cmd->duration_ms;
        if (d > ALERT_CMD_DURATION_MAX_MS)
        {
            d = ALERT_CMD_DURATION_MAX_MS;
        }
        r->active   = true;
        r->type     = cmd->alert_type;
        r->severity = cmd->severity;
        r->until_ms = now_ms + d;
    }
    else if (cmd->kind == ALERT_CMD_CLEAR)
    {
        r->active = false;
    }
}

alert_t alert_remote_merge(alert_remote_t *r, const alert_t *local, uint32_t now_ms)
{
    alert_t none = {V2V_ALERT_NONE, V2V_SEVERITY_INFO, false};
    alert_t out  = (local != NULL) ? *local : none;

    if ((r == NULL) || !r->active)
    {
        return out;
    }
    if ((int32_t)(now_ms - r->until_ms) >= 0)
    {
        r->active = false; /* expired */
        return out;
    }
    if ((out.type == V2V_ALERT_NONE) || ((int)r->severity > (int)out.severity))
    {
        out.type     = r->type;
        out.severity = r->severity;
        /* out.sensor_fault stays the local one */
    }
    return out;
}
