/*
 * Quilt — everything that can be played gently. Plugin API v2 entry points.
 * DESIGN.md is the reference; this is build step 2, the scaffold.
 *
 * create_instance does all the allocation (the two contracts) and no file
 * I/O. It runs on the loader thread from host 1.7.0 and on the callback
 * before that, so it stays cheap on both. Everything else runs on the audio
 * callback: no allocation, locks or logging, except one log line for a
 * rejected `type`, which only a broken caller sends.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "quilt.h"

/* The next "key=value" pair of a voicing, or 0 at the end. No allocation. */
static int next_pair(const char **pp, char *key, int key_len, float *v) {
    const char *p = *pp;
    int n = 0;
    while (*p == ' ') p++;
    while (*p && *p != '=' && n < key_len - 1) key[n++] = *p++;
    key[n] = '\0';
    if (*p != '=') return 0;
    *v = strtof(p + 1, NULL);
    while (*p && *p != ' ') p++;
    *pp = p;
    return 1;
}

/* An instrument's CHAR and own page, as its voicing has them. */
void quilt_reset_instrument(quilt_t *q, int inst) {
    shape_t s = QUILT_INST[inst].shape;
    for (int k = 0; k < quilt_shape_key_count(s); k++) q->slot[inst][k] = quilt_shape_param(s, k)->def;
    q->charv[inst] = 0.5f;
    char key[24];
    float v;
    for (const char *p = quilt_voicing(inst); next_pair(&p, key, sizeof(key), &v);) {
        if (!strcmp(key, "char")) q->charv[inst] = v;
        else {
            int i = quilt_shape_key_in(s, key);
            if (i >= 0) q->slot[inst][i] = v;
        }
    }
}

/* A factory preset, one per TYPE option, and what turning TYPE does: the
 * instrument with its whole default sound on every page. Only VOL stays. */
void quilt_apply_preset(quilt_t *q, int preset) {
    const int inst = quilt_type_inst(preset);
    const instrument_t *in = &QUILT_INST[inst];
    q->preset = preset;
    q->type = inst;
    quilt_reset_instrument(q, inst);
    for (int i = 0; i < G_COUNT; i++)
        if (i != G_VOLUME) q->g[i] = QUILT_GLOBALS[i].def;
    q->g[G_SOFT] = in->soft;
    q->g[G_DECAY] = in->decay;
    q->g[G_SWAY] = in->sway;
    q->g[G_SPACE] = in->space;
    char key[24];
    float v;
    for (const char *p = quilt_voicing(inst); next_pair(&p, key, sizeof(key), &v);) {
        int g = quilt_global_index(key);
        if (g >= 0 && g != G_VOLUME) q->g[g] = v;
    }
}

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir;
    (void)json_defaults;
    quilt_t *q = calloc(1, sizeof(*q));
    if (!q) return NULL;
    for (int i = 0; i < G_COUNT; i++) q->g[i] = QUILT_GLOBALS[i].def;
    for (int i = 0; i < QUILT_NINST; i++) quilt_reset_instrument(q, i);
    banks_reset(&q->banks);
    waveguide_bank_reset(&q->symp);
    q->bowbody.inst = -1;
    quilt_apply_preset(q, 0);
    memcpy(q->gs, q->g, sizeof(q->g));
    q->fx.pre_s = 1.0f + q->g[G_DELAY] * 0.1f * QUILT_SR;
    if (quilt_build_contracts(q) != 0) {
        free(q);
        return NULL;
    }
    return q;
}

static void destroy_instance(void *instance) {
    quilt_t *q = instance;
    if (!q) return;
    free(q->hierarchy);
    free(q->chain_params);
    free(q);
}

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    quilt_t *q = instance;
    if (len < 2) return;
    uint8_t st = msg[0] & 0xF0;
    if (st == 0x90 && len >= 3 && msg[2] > 0) quilt_note_on(q, msg[1], msg[2]);
    else if (st == 0x80 || (st == 0x90 && len >= 3)) quilt_note_off(q, msg[1]);
    else if (st == 0xA0 && len >= 3) quilt_pressure(q, msg[1], msg[2]);   /* the pads */
    else if (st == 0xD0) quilt_pressure(q, -1, msg[1]);                   /* channel pressure */
    else if (st == 0xB0 && len >= 3) {
        if (msg[1] == 64) q->pedal = msg[2] >= 64;
        else if (msg[1] == 1) q->modwheel = (float)msg[2] / 127.0f;
        else if (msg[1] == 120 || msg[1] == 123) quilt_all_off(q);
    }
}

static int type_from(const char *val) {
    float f;
    if (!quilt_parse_value(&QUILT_TYPE_PARAM, val, &f)) return -1;
    return quilt_type_inst((int)f);
}

static void set_param(void *instance, const char *key, const char *val) {
    quilt_t *q = instance;
    float f;
    if (!key || !val) return;
    if (!strcmp(key, "type")) {
        int t = type_from(val);
        /* A new instrument arrives with its own default sound; writing the
         * one already chosen changes nothing, so a repeated write is safe. */
        if (t < 0) fprintf(stderr, "quilt: rejected type '%s'\n", val);
        else if (t != q->type) quilt_apply_preset(q, quilt_type_index(t));
        return;
    }
    if (!strcmp(key, "preset")) {
        int i = atoi(val);
        if (i >= 0 && i < QUILT_NTYPES) quilt_apply_preset(q, i);
        return;
    }
    if (!strcmp(key, "state")) {
        quilt_read_state(q, val);
        memcpy(q->gs, q->g, sizeof(q->g));   /* a recalled patch starts where it was saved */
        q->fx.pre_s = 1.0f + q->g[G_DELAY] * 0.1f * QUILT_SR;
        return;
    }
    int g = quilt_global_index(key);
    if (g >= 0) {
        if (quilt_parse_value(&QUILT_GLOBALS[g], val, &f)) q->g[g] = f;
        return;
    }
    if (!strncmp(key, "c_", 2)) {
        int i = quilt_instrument_by_slug(key + 2, (int)strlen(key + 2));
        if (i >= 0) q->charv[i] = fminf(fmaxf(strtof(val, NULL), 0.0f), 1.0f);
        return;
    }
    /* An Instrument-page key writes the active instrument's own value. A key
     * from another shape belongs to no page on screen, so it is ignored. */
    shape_t s;
    int idx = quilt_shape_key(key, &s);
    if (idx >= 0 && s == QUILT_INST[q->type].shape &&
        quilt_parse_value(quilt_shape_param(s, idx), val, &f))
        q->slot[q->type][idx] = f;
}

static int put(char *buf, int buf_len, const char *s, int n) {
    if (n >= buf_len) return -1;
    memcpy(buf, s, (size_t)n + 1);
    return n;
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    quilt_t *q = instance;
    if (!key || !buf || buf_len <= 0) return -1;
    if (!strcmp(key, "ui_hierarchy")) return put(buf, buf_len, q->hierarchy, q->hierarchy_len);
    if (!strcmp(key, "chain_params")) return put(buf, buf_len, q->chain_params, q->chain_params_len);
    if (!strcmp(key, "state")) return quilt_write_state(q, buf, buf_len);
    if (!strcmp(key, "type"))
        return quilt_format_value(&QUILT_TYPE_PARAM, (float)quilt_type_index(q->type), buf, buf_len);
    if (!strcmp(key, "preset")) return snprintf(buf, (size_t)buf_len, "%d", q->preset);
    if (!strcmp(key, "preset_count")) return snprintf(buf, (size_t)buf_len, "%d", QUILT_NTYPES);
    if (!strcmp(key, "preset_name")) {
        const char *name = QUILT_TYPE_NAMES[q->preset];
        return put(buf, buf_len, name, (int)strlen(name));
    }
    int g = quilt_global_index(key);
    if (g >= 0) return quilt_format_value(&QUILT_GLOBALS[g], q->g[g], buf, buf_len);
    if (!strncmp(key, "c_", 2)) {
        int i = quilt_instrument_by_slug(key + 2, (int)strlen(key + 2));
        if (i >= 0) return snprintf(buf, (size_t)buf_len, "%.4f", (double)q->charv[i]);
        return -1;
    }
    /* The active instrument's value; for another shape's key, its default. */
    shape_t s;
    int idx = quilt_shape_key(key, &s);
    if (idx >= 0) {
        const param_def_t *d = quilt_shape_param(s, idx);
        float v = s == QUILT_INST[q->type].shape ? q->slot[q->type][idx] : d->def;
        return quilt_format_value(d, v, buf, buf_len);
    }
    return -1;
}

/* The output stage: 8 dB of headroom below the engines, so a loud chord of
 * eight notes stays clear of the limiter, and a limiter that leaves the
 * signal alone below -6 dBFS and bends smoothly to full scale above it. */
#define HEADROOM 0.4f

static inline int16_t limit(float x) {
    float a = fabsf(x);
    if (a > 0.5f) a = 0.5f + 0.5f * tanhf((a - 0.5f) * 2.0f);
    return (int16_t)(copysignf(a, x) * 32000.0f);
}

static float volume_gain(float db) { return db <= -59.9f ? 0.0f : HEADROOM * powf(10.0f, db / 20.0f); }

static void render_block(void *instance, int16_t *out, int frames) {
    quilt_t *q = instance;
#if defined(__aarch64__)
    /* Flush denormals to zero while Quilt renders, so a long tail never falls
     * into slow subnormal arithmetic; the host's own mode is put back after. */
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr | (1ull << 24)));
#endif
    float l[QUILT_MAX_BLOCK], r[QUILT_MAX_BLOCK];
    for (int done = 0; done < frames;) {
        int n = frames - done < QUILT_MAX_BLOCK ? frames - done : QUILT_MAX_BLOCK;
        quilt_render(q, l, r, n);
        const float g0 = volume_gain(q->gs_prev[G_VOLUME]), g1 = volume_gain(q->gs[G_VOLUME]);
        for (int i = 0; i < n; i++) {
            float gain = g0 + (g1 - g0) * (float)(i + 1) / (float)n;
            out[2 * (done + i)] = limit(l[i] * gain);
            out[2 * (done + i) + 1] = limit(r[i] * gain);
        }
        done += n;
    }
#if defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr));
#endif
}

static plugin_api_v2_t api = {
    .api_version = 2,
    .create_instance = create_instance,
    .destroy_instance = destroy_instance,
    .on_midi = on_midi,
    .set_param = set_param,
    .get_param = get_param,
    .render_block = render_block,
};

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
    (void)host;
    return &api;
}
