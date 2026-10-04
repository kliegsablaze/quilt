/*
 * Quilt Probe — a throwaway sound generator that answers two host questions
 * on the device before Quilt's real scaffold is written (DESIGN.md, Host check):
 *
 *   1. Do knob labels and pages follow `type` when it changes — from the TYPE
 *      knob, and from a preset load?  Every gate is a `visible_if` on `type`
 *      itself, because the grid re-plans only when the changed key is itself a
 *      condition key (page_controller.mjs, replanIfCondition).
 *   2. Does pad pressure (poly aftertouch, 0xA0) reach the instrument?  The
 *      last pressure and a message count are published as live readouts, and
 *      the Solo Cello voice swells with pressure so it can be heard too.
 *
 * Three instruments, three presets, eight voices of plain synthesis. Not Quilt.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"

#define SR 44100.0f
#define NVOICES 8
#define TWO_PI 6.28318530718f

enum { T_PIANO, T_CELLO, T_HANDPAN, T_COUNT };
static const char *const TYPE_NAMES[T_COUNT] = { "Felt Upright", "Solo Cello", "Handpan" };

typedef struct {
    const char *name;
    int type;
} preset_t;

static const preset_t PRESETS[] = {
    { "Probe Piano",   T_PIANO },
    { "Probe Cello",   T_CELLO },
    { "Probe Handpan", T_HANDPAN },
};
#define NPRESETS ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

typedef struct {
    int active;
    int note;
    int held;
    float phase;
    float freq;
    float vel;      /* 0..1 */
    float press;    /* 0..1, from poly aftertouch */
    int got_press;
    float env;      /* decaying envelope for struck types */
    float amp;      /* smoothed output gain */
} voice_t;

typedef struct {
    int type;
    int preset;
    float c_felt, c_tasto, c_cavity;
    float p_unison, p_strike, b_bow, b_vib, h_ding, h_ring;
    float decay;
    float volume;
    int pressure;   /* last poly or channel pressure, 0..127 */
    int at_count;   /* pressure messages received */
    voice_t v[NVOICES];
} probe_t;

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir;
    (void)json_defaults;
    probe_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->type = T_PIANO;
    p->c_felt = p->c_tasto = p->c_cavity = 0.5f;
    p->p_unison = p->p_strike = p->b_bow = p->b_vib = p->h_ding = p->h_ring = 0.5f;
    p->decay = 0.5f;
    p->volume = 0.7f;
    return p;
}

static void destroy_instance(void *instance) { free(instance); }

static void note_on(probe_t *p, int note, int vel) {
    int slot = -1;
    for (int i = 0; i < NVOICES; i++)
        if (!p->v[i].active) { slot = i; break; }
    if (slot < 0) {                     /* steal the quietest */
        float lo = 1e9f;
        for (int i = 0; i < NVOICES; i++)
            if (p->v[i].amp < lo) { lo = p->v[i].amp; slot = i; }
    }
    voice_t *v = &p->v[slot];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->held = 1;
    v->note = note;
    v->freq = 440.0f * powf(2.0f, (note - 69) / 12.0f);
    v->vel = vel / 127.0f;
    v->env = 1.0f;
}

static void note_off(probe_t *p, int note) {
    for (int i = 0; i < NVOICES; i++)
        if (p->v[i].active && p->v[i].note == note) p->v[i].held = 0;
}

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    probe_t *p = instance;
    if (len < 2) return;
    uint8_t st = msg[0] & 0xF0;
    if (st == 0x90 && len >= 3 && msg[2] > 0) note_on(p, msg[1], msg[2]);
    else if (st == 0x80 || (st == 0x90 && len >= 3)) note_off(p, msg[1]);
    else if (st == 0xA0 && len >= 3) {              /* poly aftertouch */
        p->pressure = msg[2];
        p->at_count = (p->at_count + 1) % 1000;
        for (int i = 0; i < NVOICES; i++)
            if (p->v[i].active && p->v[i].note == msg[1]) {
                p->v[i].press = msg[2] / 127.0f;
                p->v[i].got_press = 1;
            }
    } else if (st == 0xD0) {                        /* channel pressure */
        p->pressure = msg[1];
        p->at_count = (p->at_count + 1) % 1000;
        for (int i = 0; i < NVOICES; i++)
            if (p->v[i].active) {
                p->v[i].press = msg[1] / 127.0f;
                p->v[i].got_press = 1;
            }
    }
}

static int type_from(const char *val) {
    for (int i = 0; i < T_COUNT; i++)
        if (strcmp(val, TYPE_NAMES[i]) == 0) return i;
    char *end;
    long n = strtol(val, &end, 10);
    if (end != val && *end == '\0' && n >= 0 && n < T_COUNT) return (int)n;
    return -1;
}

/* Minimal reader for the module's own flat state blob: "key":number pairs. */
static int json_num(const char *json, const char *key, float *out) {
    char pat[48];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *s = strstr(json, pat);
    if (!s) return 0;
    *out = strtof(s + strlen(pat), NULL);
    return 1;
}

static float *float_param(probe_t *p, const char *key) {
    if (!strcmp(key, "c_felt")) return &p->c_felt;
    if (!strcmp(key, "c_tasto")) return &p->c_tasto;
    if (!strcmp(key, "c_cavity")) return &p->c_cavity;
    if (!strcmp(key, "p_unison")) return &p->p_unison;
    if (!strcmp(key, "p_strike")) return &p->p_strike;
    if (!strcmp(key, "b_bow")) return &p->b_bow;
    if (!strcmp(key, "b_vib")) return &p->b_vib;
    if (!strcmp(key, "h_ding")) return &p->h_ding;
    if (!strcmp(key, "h_ring")) return &p->h_ring;
    if (!strcmp(key, "decay")) return &p->decay;
    if (!strcmp(key, "volume")) return &p->volume;
    return NULL;
}

static const char *const FLOAT_KEYS[] = {
    "c_felt", "c_tasto", "c_cavity", "p_unison", "p_strike", "b_bow",
    "b_vib", "h_ding", "h_ring", "decay", "volume",
};
#define NFLOAT_KEYS ((int)(sizeof(FLOAT_KEYS) / sizeof(FLOAT_KEYS[0])))

static void set_param(void *instance, const char *key, const char *val) {
    probe_t *p = instance;
    if (!key || !val) return;
    if (!strcmp(key, "type")) {
        int t = type_from(val);
        if (t >= 0) p->type = t;
        else fprintf(stderr, "quiltprobe: rejected type '%s'\n", val);
        return;
    }
    if (!strcmp(key, "preset")) {
        int i = atoi(val);
        if (i >= 0 && i < NPRESETS) {
            p->preset = i;
            p->type = PRESETS[i].type;
        }
        return;
    }
    if (!strcmp(key, "state")) {
        float f;
        if (json_num(val, "type", &f) && f >= 0 && f < T_COUNT) p->type = (int)f;
        if (json_num(val, "preset", &f) && f >= 0 && f < NPRESETS) p->preset = (int)f;
        for (int i = 0; i < NFLOAT_KEYS; i++)
            if (json_num(val, FLOAT_KEYS[i], &f)) *float_param(p, FLOAT_KEYS[i]) = clamp01(f);
        return;
    }
    float *f = float_param(p, key);
    if (f) *f = clamp01(strtof(val, NULL));
    /* pressure and at_count are readouts: writes mean nothing. */
}

static const char UI_HIERARCHY[] =
    "{\"modes\":null,\"levels\":{"
    "\"root\":{\"label\":\"Quilt Probe\","
    "\"list_param\":\"preset\",\"count_param\":\"preset_count\",\"name_param\":\"preset_name\","
    "\"knobs\":[\"type\",\"c_felt\",\"c_tasto\",\"c_cavity\",\"decay\",\"pressure\",\"at_count\",\"volume\"],"
    "\"params\":["
    "{\"key\":\"type\",\"label\":\"Instrument\"},"
    "{\"key\":\"c_felt\",\"label\":\"Felt\",\"visible_if\":{\"param\":\"type\",\"equals\":\"Felt Upright\"}},"
    "{\"key\":\"c_tasto\",\"label\":\"Tasto\",\"visible_if\":{\"param\":\"type\",\"equals\":\"Solo Cello\"}},"
    "{\"key\":\"c_cavity\",\"label\":\"Cavity\",\"visible_if\":{\"param\":\"type\",\"equals\":\"Handpan\"}},"
    "{\"key\":\"decay\",\"label\":\"Decay\"},"
    "{\"key\":\"pressure\",\"label\":\"Pressure\"},"
    "{\"key\":\"at_count\",\"label\":\"Pressure msgs\"},"
    "{\"key\":\"volume\",\"label\":\"Volume\"},"
    "{\"level\":\"piano\",\"label\":\"Piano\"},"
    "{\"level\":\"cello\",\"label\":\"Cello\"},"
    "{\"level\":\"handpan\",\"label\":\"Handpan\"}"
    "]},"
    "\"piano\":{\"label\":\"Piano\",\"visible_if\":{\"param\":\"type\",\"equals\":\"Felt Upright\"},"
    "\"knobs\":[\"p_unison\",\"p_strike\"],\"params\":[\"p_unison\",\"p_strike\"]},"
    "\"cello\":{\"label\":\"Cello\",\"visible_if\":{\"param\":\"type\",\"equals\":\"Solo Cello\"},"
    "\"knobs\":[\"b_bow\",\"b_vib\"],\"params\":[\"b_bow\",\"b_vib\"]},"
    "\"handpan\":{\"label\":\"Handpan\",\"visible_if\":{\"param\":\"type\",\"equals\":\"Handpan\"},"
    "\"knobs\":[\"h_ding\",\"h_ring\"],\"params\":[\"h_ding\",\"h_ring\"]}"
    "}}";

#define FLT(k, n, s) \
    "{\"key\":\"" k "\",\"name\":\"" n "\",\"short_name\":\"" s "\",\"type\":\"float\"," \
    "\"min\":0,\"max\":1,\"step\":0.01,\"unit\":\"%\"}"

static const char CHAIN_PARAMS_HEAD[] =
    "["
    "{\"key\":\"type\",\"name\":\"Instrument\",\"short_name\":\"TYPE\",\"type\":\"enum\","
    "\"options\":[\"Felt Upright\",\"Solo Cello\",\"Handpan\"],\"options_as_string\":true},"
    FLT("c_felt", "Felt", "FELT") ","
    FLT("c_tasto", "Tasto", "TASTO") ","
    FLT("c_cavity", "Cavity", "CAVITY") ","
    FLT("p_unison", "Unison", "UNISON") ","
    FLT("p_strike", "Strike", "STRIKE") ","
    FLT("b_bow", "Bow", "BOW") ","
    FLT("b_vib", "Vibrato", "VIB") ","
    FLT("h_ding", "Ding", "DING") ","
    FLT("h_ring", "Ring", "RING") ","
    FLT("decay", "Decay", "DECAY") ","
    FLT("volume", "Volume", "VOL") ","
    "{\"key\":\"pressure\",\"name\":\"Pressure\",\"short_name\":\"PRES\",\"type\":\"int\","
    "\"min\":0,\"max\":127,\"access\":\"read\",\"live\":true},"
    "{\"key\":\"at_count\",\"name\":\"Pressure msgs\",\"short_name\":\"MSGS\",\"type\":\"int\","
    "\"min\":0,\"max\":999,\"display\":\"big\",\"access\":\"read\",\"live\":true},";

static int put(char *buf, int buf_len, const char *s) {
    int n = (int)strlen(s);
    if (n >= buf_len) return -1;
    memcpy(buf, s, (size_t)n + 1);
    return n;
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    probe_t *p = instance;
    if (!key || !buf || buf_len <= 0) return -1;
    if (!strcmp(key, "ui_hierarchy")) return put(buf, buf_len, UI_HIERARCHY);
    if (!strcmp(key, "chain_params")) {
        int n = snprintf(buf, (size_t)buf_len,
                         "%s{\"key\":\"preset\",\"name\":\"Preset\",\"type\":\"int\",\"min\":0,\"max\":%d}]",
                         CHAIN_PARAMS_HEAD, NPRESETS - 1);
        return (n < 0 || n >= buf_len) ? -1 : n;
    }
    if (!strcmp(key, "type")) return put(buf, buf_len, TYPE_NAMES[p->type]);
    if (!strcmp(key, "preset")) return snprintf(buf, (size_t)buf_len, "%d", p->preset);
    if (!strcmp(key, "preset_count")) return snprintf(buf, (size_t)buf_len, "%d", NPRESETS);
    if (!strcmp(key, "preset_name")) return put(buf, buf_len, PRESETS[p->preset].name);
    if (!strcmp(key, "pressure")) return snprintf(buf, (size_t)buf_len, "%d", p->pressure);
    if (!strcmp(key, "at_count")) return snprintf(buf, (size_t)buf_len, "%d", p->at_count);
    if (!strcmp(key, "state")) {
        int n = snprintf(buf, (size_t)buf_len, "{\"type\":%d,\"preset\":%d", p->type, p->preset);
        for (int i = 0; i < NFLOAT_KEYS && n > 0 && n < buf_len; i++)
            n += snprintf(buf + n, (size_t)(buf_len - n), ",\"%s\":%.4f",
                          FLOAT_KEYS[i], (double)*float_param(p, FLOAT_KEYS[i]));
        if (n > 0 && n < buf_len) n += snprintf(buf + n, (size_t)(buf_len - n), "}");
        return (n < 0 || n >= buf_len) ? -1 : n;
    }
    float *f = float_param(p, key);
    if (f) return snprintf(buf, (size_t)buf_len, "%.4f", (double)*f);
    return -1;
}

static void render_block(void *instance, int16_t *out, int frames) {
    probe_t *p = instance;
    /* Struck types ring for 0.3..4 s; the cello sustains while held. */
    float t60 = 0.3f + 3.7f * p->decay;
    float dec = powf(0.001f, 1.0f / (t60 * SR));
    float rel = powf(0.001f, 1.0f / (0.15f * SR));
    float smooth = 1.0f - expf(-1.0f / (0.01f * SR));
    for (int n = 0; n < frames; n++) {
        float mix = 0.0f;
        for (int i = 0; i < NVOICES; i++) {
            voice_t *v = &p->v[i];
            if (!v->active) continue;
            float s1 = sinf(v->phase), target;
            float s;
            switch (p->type) {
            case T_CELLO:
                /* Pressure is the bow: with no pressure yet, velocity holds it. */
                target = v->held ? (v->got_press ? v->press : v->vel) : 0.0f;
                s = s1 + 0.3f * sinf(2.0f * v->phase) + 0.15f * sinf(3.0f * v->phase);
                break;
            case T_HANDPAN:
                v->env *= dec;          /* a handpan rings on, held or not */
                target = v->vel * v->env;
                s = s1 + 0.4f * p->h_ding * sinf(2.0f * v->phase);
                break;
            default:
                v->env *= v->held ? dec : rel;
                target = v->vel * v->env;
                s = s1;
                break;
            }
            v->amp += (target - v->amp) * smooth;
            mix += s * v->amp * 0.25f;
            v->phase += TWO_PI * v->freq / SR;
            if (v->phase > TWO_PI) v->phase -= TWO_PI;
            if (!v->held && v->amp < 1e-4f && target < 1e-4f) v->active = 0;
            if (p->type != T_CELLO && v->env < 1e-4f) v->active = 0;
        }
        float y = tanhf(mix * p->volume);
        int16_t q = (int16_t)(y * 32000.0f);
        out[2 * n] = q;
        out[2 * n + 1] = q;
    }
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
