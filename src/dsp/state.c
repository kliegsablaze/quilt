/*
 * `state`: one flat JSON object holding the Main and Effects keys, and the
 * CHAR value and own page values of every instrument TYPE offers. Reading
 * it sets each value directly and never loads a voicing, so a saved sound
 * comes back exactly (DESIGN.md, Every instrument starts beautiful).
 *
 *   {"v":2,"type":"Marimba","preset":42,"soft":0.6000,...,
 *    "c_felt_upright":0.5000,...,"marimba.m_spot":0.3000,...}
 *
 * The reader is one pass over "key":value pairs and ignores keys it does not
 * know, so older and newer blobs both load. Version 1 had one preset per
 * instrument, so its "preset" is a TYPE index; it maps to that instrument's
 * first preset.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quilt.h"

typedef struct {
    char *buf;
    int len, cap;
} out_t;

static void put(out_t *o, const char *fmt, const char *key, double v) {
    if (o->len < 0) return;
    int n = snprintf(o->buf + o->len, (size_t)(o->cap - o->len), fmt, key, v);
    o->len = (n < 0 || o->len + n >= o->cap) ? -1 : o->len + n;
}

int quilt_write_state(const quilt_t *q, char *buf, int buf_len) {
    out_t o = { buf, 0, buf_len };
    int n = snprintf(buf, (size_t)buf_len, "{\"v\":2,\"type\":\"%s\",\"preset\":%d",
                     QUILT_INST[q->type].name, q->preset);
    o.len = (n < 0 || n >= buf_len) ? -1 : n;
    put(&o, ",\"%s\":%.1f", "trim", q->trim);
    for (int i = 0; i < G_COUNT; i++) put(&o, ",\"%s\":%.4f", QUILT_GLOBALS[i].key, q->g[i]);
    for (int t = 0; t < QUILT_NTYPES; t++) {
        int i = quilt_type_inst(t);
        put(&o, ",\"c_%s\":%.4f", QUILT_INST[i].slug, q->charv[i]);
    }
    for (int t = 0; t < QUILT_NTYPES; t++) {
        int i = quilt_type_inst(t);
        shape_t s = QUILT_INST[i].shape;
        for (int k = 0; k < quilt_shape_key_count(s); k++) {
            char key[96];
            snprintf(key, sizeof(key), "%s.%s", QUILT_INST[i].slug, quilt_shape_param(s, k)->key);
            put(&o, ",\"%s\":%.4f", key, q->slot[i][k]);
        }
    }
    if (o.len >= 0 && o.len + 2 <= o.cap) {
        buf[o.len++] = '}';
        buf[o.len] = '\0';
        return o.len;
    }
    return -1;
}

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

static void apply(quilt_t *q, int *ver, const char *key, int klen, const char *sval, int slen, float num,
                  int is_num) {
    char k[96];
    if (klen <= 0 || klen >= (int)sizeof(k)) return;
    memcpy(k, key, (size_t)klen);
    k[klen] = '\0';

    if (!strcmp(k, "type")) {
        char name[64];
        int t = -1;
        if (!is_num && slen > 0 && slen < (int)sizeof(name)) {
            memcpy(name, sval, (size_t)slen);
            name[slen] = '\0';
            t = quilt_instrument_by_name(name);
        } else if (is_num && num >= 0 && num < QUILT_NTYPES) {
            t = quilt_type_inst((int)num);
        }
        if (t >= 0 && quilt_type_index(t) >= 0) q->type = t;
        return;
    }
    if (!is_num) return;
    if (!strcmp(k, "v")) { *ver = (int)num; return; }
    if (!strcmp(k, "preset")) {
        if (*ver < 2) num *= QUILT_PER_TYPE;
        if (num >= 0 && num < QUILT_NPRESETS) q->preset = (int)num;
        return;
    }
    if (!strcmp(k, "trim")) { q->trim = clampf(num, -12.0f, 12.0f); return; }
    int g = quilt_global_index(k);
    if (g >= 0) { q->g[g] = clampf(num, QUILT_GLOBALS[g].min, QUILT_GLOBALS[g].max); return; }
    if (!strncmp(k, "c_", 2)) {
        int i = quilt_instrument_by_slug(k + 2, klen - 2);
        if (i >= 0) q->charv[i] = clampf(num, 0.0f, 1.0f);
        return;
    }
    const char *dot = memchr(k, '.', (size_t)klen);
    if (dot) {
        int i = quilt_instrument_by_slug(k, (int)(dot - k));
        if (i < 0) return;
        shape_t s = QUILT_INST[i].shape;
        int idx = quilt_shape_key_in(s, dot + 1);
        if (idx < 0) return;
        const param_def_t *d = quilt_shape_param(s, idx);
        float v = clampf(num, d->min, d->max);
        q->slot[i][idx] = d->kind == PK_FLOAT ? v : (float)(int)(v + 0.5f);
    }
}

void quilt_read_state(quilt_t *q, const char *s) {
    const char *p = strchr(s, '{');
    int ver = 1;
    if (!p) return;
    q->trim = 0.0f;   /* a blob carries its own trim, and one from before trims has none */
    p++;
    for (;;) {
        while (*p && *p != '"' && *p != '}') p++;
        if (*p != '"') return;
        const char *key = ++p;
        while (*p && *p != '"') p++;
        if (!*p) return;
        int klen = (int)(p - key);
        p++;
        while (*p == ' ' || *p == ':') p++;
        if (*p == '"') {
            const char *val = ++p;
            while (*p && *p != '"') p++;
            if (!*p) return;
            apply(q, &ver, key, klen, val, (int)(p - val), 0.0f, 0);
            p++;
        } else {
            char *end;
            float num = strtof(p, &end);
            if (end == p) {               /* true, null, a nested value: skip it */
                int depth = 0;
                for (; *p; p++) {
                    if (*p == '"') {
                        while (*++p && *p != '"') {}
                        if (!*p) return;
                    } else if (*p == '{' || *p == '[') depth++;
                    else if (*p == '}' || *p == ']') { if (depth-- == 0) break; }
                    else if (*p == ',' && depth == 0) break;
                }
            } else {
                apply(q, &ver, key, klen, NULL, 0, num, 1);
                p = end;
            }
        }
        while (*p == ' ') p++;
        if (*p == ',') p++;
        else return;
    }
}
