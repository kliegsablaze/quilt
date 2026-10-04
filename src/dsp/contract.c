/*
 * The two contracts a sound generator must serve from get_param, since the
 * host ignores a hierarchy in module.json (DESIGN.md, Implementation notes):
 *
 *   chain_params  every key once, with its cell word and header name.
 *   ui_hierarchy  Main, one Instrument level per instrument, the organ's
 *                 Drawbars and Effects. Every gate is `visible_if` on `type`
 *                 itself, because the grid re-plans only when the key just
 *                 written is a condition key (DESIGN.md, Host check).
 *
 * Built once per instance in create_instance, which runs off the audio
 * callback on hosts from 1.7.0. Both stay well under the 128 KB buffers.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quilt.h"

typedef struct {
    char *s;
    int len, cap, failed;
} sb_t;

static void sb_printf(sb_t *b, const char *fmt, ...) {
    if (b->failed) return;
    for (;;) {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(b->s + b->len, (size_t)(b->cap - b->len), fmt, ap);
        va_end(ap);
        if (n < 0) { b->failed = 1; return; }
        if (b->len + n < b->cap) { b->len += n; return; }
        int cap = b->cap * 2 + n;
        char *s = realloc(b->s, (size_t)cap);
        if (!s) { b->failed = 1; return; }
        b->s = s;
        b->cap = cap;
    }
}

static void param_json(sb_t *b, const param_def_t *d) {
    sb_printf(b, "{\"key\":\"%s\",\"name\":\"%s\",\"short_name\":\"%s\",", d->key, d->name, d->cell);
    if (d->kind == PK_ENUM) {
        sb_printf(b, "\"type\":\"enum\",\"options\":[");
        for (int i = 0; i < d->noptions; i++)
            sb_printf(b, "%s\"%s\"", i ? "," : "", d->options[i]);
        sb_printf(b, "],\"options_as_string\":true}");
    } else if (d->kind == PK_INT) {
        sb_printf(b, "\"type\":\"int\",\"min\":%d,\"max\":%d}", (int)d->min, (int)d->max);
    } else if (d->unit) {
        sb_printf(b, "\"type\":\"float\",\"min\":%g,\"max\":%g,\"step\":0.5,\"unit\":\"%s\"}",
                  (double)d->min, (double)d->max, d->unit);
    } else {
        sb_printf(b, "\"type\":\"float\",\"min\":0,\"max\":1,\"step\":0.01,\"unit\":\"%%\"}");
    }
}

static void gate(sb_t *b, const char *type_name) {
    sb_printf(b, ",\"visible_if\":{\"param\":\"type\",\"equals\":\"%s\"}", type_name);
}

/* Only the instruments TYPE offers have keys, pages and gates; the first of
 * each shape names the shape's keys in chain_params. */
static int shape_listed_before(int t) {
    for (int j = 0; j < t; j++)
        if (QUILT_INST[quilt_type_inst(j)].shape == QUILT_INST[quilt_type_inst(t)].shape) return 1;
    return 0;
}

static int shape_offered(shape_t s) {
    for (int t = 0; t < QUILT_NTYPES; t++)
        if (QUILT_INST[quilt_type_inst(t)].shape == s) return 1;
    return 0;
}

static void build_chain_params(sb_t *b) {
    sb_printf(b, "[");
    param_json(b, &QUILT_TYPE_PARAM);
    for (int i = 0; i < G_COUNT; i++) {
        sb_printf(b, ",");
        param_json(b, &QUILT_GLOBALS[i]);
    }
    for (int t = 0; t < QUILT_NTYPES; t++) {
        const instrument_t *in = &QUILT_INST[quilt_type_inst(t)];
        char key[48];
        snprintf(key, sizeof(key), "c_%s", in->slug);
        param_def_t c = { key, in->char_cell, in->char_name, PK_FLOAT, 0, 1, 0.5f, NULL, NULL, 0 };
        sb_printf(b, ",");
        param_json(b, &c);
    }
    for (int t = 0; t < QUILT_NTYPES; t++) {
        if (shape_listed_before(t)) continue;
        shape_t s = QUILT_INST[quilt_type_inst(t)].shape;
        for (int k = 0; k < quilt_shape_key_count(s); k++) {
            sb_printf(b, ",");
            param_json(b, quilt_shape_param(s, k));
        }
    }
    sb_printf(b, ",{\"key\":\"preset\",\"name\":\"Preset\",\"type\":\"int\",\"min\":0,\"max\":%d}]",
              QUILT_NTYPES - 1);
}

static void key_list(sb_t *b, const char *first, const param_def_t *keys, int n) {
    sb_printf(b, "[");
    if (first) sb_printf(b, "\"%s\"", first);
    for (int i = 0; i < n; i++) sb_printf(b, "%s\"%s\"", (first || i) ? "," : "", keys[i].key);
    sb_printf(b, "]");
}

static void build_hierarchy(sb_t *b) {
    sb_printf(b, "{\"modes\":null,\"levels\":{");

    /* Main: TYPE SOFT <CHAR> DECAY / SWAY TONE SPACE VOL. All but one of the
     * CHAR keys are hidden at any moment, and the planner closes them up.
     * Every key is on exactly one page (DESIGN.md, One place for everything). */
    sb_printf(b, "\"root\":{\"label\":\"Quilt\",\"list_param\":\"preset\","
                 "\"count_param\":\"preset_count\",\"name_param\":\"preset_name\",\"knobs\":["
                 "\"type\",\"soft\"");
    for (int t = 0; t < QUILT_NTYPES; t++) sb_printf(b, ",\"c_%s\"", QUILT_INST[quilt_type_inst(t)].slug);
    sb_printf(b, ",\"decay\",\"sway\",\"tone\",\"space\",\"volume\"],\"params\":["
                 "{\"key\":\"type\"},{\"key\":\"soft\"}");
    for (int t = 0; t < QUILT_NTYPES; t++) {
        const instrument_t *in = &QUILT_INST[quilt_type_inst(t)];
        sb_printf(b, ",{\"key\":\"c_%s\"", in->slug);
        gate(b, in->name);
        sb_printf(b, "}");
    }
    sb_printf(b, ",{\"key\":\"decay\"},{\"key\":\"sway\"},{\"key\":\"tone\"},"
                 "{\"key\":\"space\"},{\"key\":\"volume\"}");
    for (int t = 0; t < QUILT_NTYPES; t++) {
        const instrument_t *in = &QUILT_INST[quilt_type_inst(t)];
        sb_printf(b, ",{\"level\":\"i_%s\",\"label\":\"%s\"}", in->slug, in->name);
    }
    for (int s = 0; s < SH_COUNT; s++)
        if (QUILT_SHAPES[s].extra_level && shape_offered((shape_t)s))
            sb_printf(b, ",{\"level\":\"%s\",\"label\":\"%s\"}",
                      QUILT_SHAPES[s].extra_level, QUILT_SHAPES[s].extra_label);
    sb_printf(b, ",{\"level\":\"fx\",\"label\":\"Effects\"}]}");

    /* One Instrument level per instrument, titled with its name, so its labels
     * are its own. Its CHAR lives on Main only. */
    for (int t = 0; t < QUILT_NTYPES; t++) {
        const instrument_t *in = &QUILT_INST[quilt_type_inst(t)];
        const shape_def_t *sh = &QUILT_SHAPES[in->shape];
        sb_printf(b, ",\"i_%s\":{\"label\":\"%s\"", in->slug, in->name);
        gate(b, in->name);
        sb_printf(b, ",\"knobs\":");
        key_list(b, NULL, sh->keys, sh->nkeys);
        sb_printf(b, ",\"params\":");
        key_list(b, NULL, sh->keys, sh->nkeys);
        sb_printf(b, "}");
    }

    /* A shape's second page, shown for each instrument of that shape. */
    for (int s = 0; s < SH_COUNT; s++) {
        const shape_def_t *sh = &QUILT_SHAPES[s];
        if (!sh->extra_level) continue;
        for (int t = 0; t < QUILT_NTYPES; t++) {
            const instrument_t *in = &QUILT_INST[quilt_type_inst(t)];
            if (in->shape != (shape_t)s) continue;
            sb_printf(b, ",\"%s\":{\"label\":\"%s\"", sh->extra_level, sh->extra_label);
            gate(b, in->name);
            sb_printf(b, ",\"knobs\":");
            key_list(b, NULL, sh->extra, sh->nextra);
            sb_printf(b, ",\"params\":");
            key_list(b, NULL, sh->extra, sh->nextra);
            sb_printf(b, "}");
            break;   /* one gate per level: an extra page serves one instrument */
        }
    }

    /* Effects: the plate and DRIVE on top, SPEED below. SPEED is hidden where
     * SWAY is not a tremolo (the vibraphone, whose fan speed is its MOTOR),
     * so no knob is ever shown that does nothing. */
    sb_printf(b, ",\"fx\":{\"label\":\"Effects\",\"knobs\":[\"size\",\"dark\",\"delay\",\"drive\","
                 "\"speed\"],\"params\":[\"size\",\"dark\",\"delay\",\"drive\",{\"key\":\"speed\"");
    for (int t = 0; t < QUILT_NTYPES; t++) {
        const int i = quilt_type_inst(t);
        if (quilt_sway_is_tremolo(i)) continue;
        /* A gate is one comparison, so this serves one such instrument. A
         * second (the organ's Leslie) will need SPEED split per instrument,
         * as CHAR is; tests/run.sh fails until then. */
        sb_printf(b, ",\"visible_if\":{\"param\":\"type\",\"not_equals\":\"%s\"}", QUILT_INST[i].name);
        break;
    }
    sb_printf(b, "}]}");
    sb_printf(b, "}}");
}

int quilt_build_contracts(quilt_t *q) {
    sb_t a = { malloc(16384), 0, 16384, 0 }, h = { malloc(16384), 0, 16384, 0 };
    if (!a.s || !h.s) a.failed = 1;
    if (!a.failed) build_chain_params(&a);
    if (!a.failed && h.s) build_hierarchy(&h);
    if (a.failed || h.failed || !h.s) {
        free(a.s);
        free(h.s);
        return -1;
    }
    q->chain_params = a.s;
    q->chain_params_len = a.len;
    q->hierarchy = h.s;
    q->hierarchy_len = h.len;
    return 0;
}
