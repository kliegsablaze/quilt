/*
 * The Modulation page: four modulators, each a source and two destinations
 * (DESIGN.md, Modulation).
 *
 *   KIND  Velocity, MPE, LFO or Envelope. Velocity, MPE and Envelope are
 *         every note's own; an LFO is one for all of them.
 *   AIM   any knob on any other page, TYPE included, or another modulator's
 *         own knobs. A word from the Instrument page (Edge, Body...) is
 *         whichever knob has that word on the instrument playing.
 *   DEPTH how far, either way: full depth sweeps the whole knob.
 *
 * Nothing here writes what a knob is set to. Each block, the values a
 * modulator moves are laid over the knobs while a note renders, then put
 * back (mod_apply, mod_restore), so every page keeps showing what was set.
 * A note's own sources move it alone; what every note shares (the room, the
 * Leslie, VOL) follows the newest note.
 *
 * The modulators are worked out in order, each with the others as they stand,
 * so a ring (1 into 2 into 3 into 4 into 1) hears its last link one block late
 * and cannot run away.
 */
#include <math.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "quilt.h"

#define F(k, c, n, d) { k, c, n, PK_FLOAT, 0.0f, 1.0f, d, NULL, NULL, 0 }
#define B(k, c, n, d) { k, c, n, PK_FLOAT, -1.0f, 1.0f, d, NULL, NULL, 0 }
#define E(k, c, n, opts, d) \
    { k, c, n, PK_ENUM, 0.0f, (float)(sizeof(opts) / sizeof(opts[0]) - 1), d, NULL, \
      opts, (int)(sizeof(opts) / sizeof(opts[0])) }

static const char *const SELECT_OPTS[] = { "1", "2", "3", "4" };
const param_def_t QUILT_MOD_SELECT = E("mod", "Mod", "Modulator", SELECT_OPTS, 0.0f);

/* Where nothing is worth turning, a cell that draws nothing and does nothing,
 * so the destinations stay on the bottom row (the host closes up a hidden
 * cell). Its one option is a space, which the host draws as no label. */
static const char *const GAP_OPTS[] = { " " };
const param_def_t QUILT_MOD_GAPS[2] = { E("gap1", " ", " ", GAP_OPTS, 0.0f), E("gap2", " ", " ", GAP_OPTS, 0.0f) };

static const char *const KIND_OPTS[] = { "Velocity", "MPE", "LFO", "Envelope" };
static const char *const SHAPE_OPTS[] = { "Sine", "Triangle", "Saw", "Ramp", "Square", "Random", "Drift" };
static const char *const AXIS_OPTS[] = { "Press", "Slide", "Bend" };

/* Instrument-page words, each one the cell word of a knob on some page
 * shape; tests/test_quilt.c checks they match. */
static const char *const WORDS[] = {
    "Split", "Stiff", "Spot", "Body", "Noise", "Damp", "Pedal", "Edge", "Press", "Swell", "Bite", "Bow",
    "Blur", "Hit", "Onset", "Air", "Ping", "Tail", "Click", "Leak", "Slow", "Fast", "Tune", "Tine",
    "Crowd", "16'", "5.3'", "8'", "4'", "2.7'", "2'", "1.6'", "1.3'", "1'",
};
#define NWORDS ((int)(sizeof(WORDS) / sizeof(WORDS[0])))
_Static_assert(sizeof(WORDS) / sizeof(WORDS[0]) <= QUILT_MAX_WORDS, "the word table fits");

#define MOD_AIMS(n) "Mod " #n " Kind", "Mod " #n " Shape", "Mod " #n " Rate", "Mod " #n " Rise", \
    "Mod " #n " Fall", "Mod " #n " Axis", "Mod " #n " Lag", "Mod " #n " Aim 1", "Mod " #n " Depth 1", \
    "Mod " #n " Aim 2", "Mod " #n " Depth 2"

/* The destinations, in page order: Main, the Instrument page's words,
 * Effects, then the modulators' own knobs in MP_ order. */
static const char *const AIMS[] = {
    "None", "Type", "Soft", "Character", "Decay", "Sway", "Tone", "Space", "Volume",
    "Split", "Stiff", "Spot", "Body", "Noise", "Damp", "Pedal", "Edge", "Press", "Swell", "Bite", "Bow",
    "Blur", "Hit", "Onset", "Air", "Ping", "Tail", "Click", "Leak", "Slow", "Fast", "Tune", "Tine",
    "Crowd", "16'", "5.3'", "8'", "4'", "2.7'", "2'", "1.6'", "1.3'", "1'",
    "Size", "Dark", "Delay", "Drive", "Speed",
    MOD_AIMS(1), MOD_AIMS(2), MOD_AIMS(3), MOD_AIMS(4),
};
#define NAIMS ((int)(sizeof(AIMS) / sizeof(AIMS[0])))
#define A_WORDS 9
#define A_FX (A_WORDS + NWORDS)
#define A_MODS (A_FX + 5)
_Static_assert(A_MODS + QUILT_MODS * MP_COUNT == NAIMS, "every destination is named");

static const int MAIN_G[] = { -1, -1, G_SOFT, -1, G_DECAY, G_SWAY, G_TONE, G_SPACE, G_VOLUME };
static const int FX_G[] = { G_SIZE, G_DARK, G_DELAY, G_DRIVE, G_SPEED };

const param_def_t QUILT_MOD_PARAMS[MP_COUNT] = {
    [MP_KIND] = E("kind", "Kind", "Kind", KIND_OPTS, 0.0f),
    [MP_SHAPE] = E("shape", "Shape", "LFO Shape", SHAPE_OPTS, 0.0f),
    [MP_RATE] = B("rate", "Rate", "LFO Rate", 0.23f),
    [MP_RISE] = F("rise", "Rise", "Rise Time", 0.5f),
    [MP_FALL] = F("fall", "Fall", "Fall Time", 0.5f),
    [MP_AXIS] = E("axis", "Axis", "MPE Axis", AXIS_OPTS, 0.0f),
    [MP_LAG] = F("lag", "Lag", "Smoothing", 0.3f),
    [MP_AIM1] = E("aim1", "Aim", "Destination 1", AIMS, 0.0f),
    [MP_DEPTH1] = B("depth1", "Depth", "Depth 1", 0.0f),
    [MP_AIM2] = E("aim2", "Aim", "Destination 2", AIMS, 0.0f),
    [MP_DEPTH2] = B("depth2", "Depth", "Depth 2", 0.0f),
};

/* A fresh modulator of each kind, so the KIND knob shows all four. */
static const int DEFAULT_KIND[QUILT_MODS] = { MK_LFO, MK_ENV, MK_VELOCITY, MK_MPE };

static const param_def_t CHAR_DEF = F("char", "Char", "Character", 0.5f);

/* ---- names and keys ---- */

int quilt_aim_count(void) { return NAIMS; }
const char *quilt_aim_name(int i) { return i >= 0 && i < NAIMS ? AIMS[i] : ""; }
int quilt_word_count(void) { return NWORDS; }
const char *quilt_word(int i) { return WORDS[i]; }

int quilt_mod_key(const char *key, int *mod_out) {
    if (strncmp(key, "mod", 3) || key[3] < '1' || key[3] > '0' + QUILT_MODS || key[4] != '_') return -1;
    for (int p = 0; p < MP_COUNT; p++)
        if (!strcmp(key + 5, QUILT_MOD_PARAMS[p].key)) { *mod_out = key[3] - '1'; return p; }
    return -1;
}

void quilt_mod_reset(quilt_t *q) {
    for (int m = 0; m < QUILT_MODS; m++) {
        for (int p = 0; p < MP_COUNT; p++) q->mod[m][p] = QUILT_MOD_PARAMS[p].def;
        q->mod[m][MP_KIND] = (float)DEFAULT_KIND[m];
    }
}

void quilt_mod_init(quilt_t *q) {
    quilt_mod_reset(q);
    for (int m = 0; m < QUILT_MODS; m++) {
        memcpy(q->run[m].e, q->mod[m], sizeof(q->run[m].e));
        q->run[m].rng = 0x2545F491u * (uint32_t)(m + 1);
    }
    for (int s = 0; s < SH_COUNT; s++)
        for (int w = 0; w < NWORDS; w++) {
            q->word_slot[s][w] = -1;
            for (int k = 0; k < quilt_shape_key_count((shape_t)s); k++)
                if (!strcmp(quilt_shape_param((shape_t)s, k)->cell, WORDS[w])) q->word_slot[s][w] = (signed char)k;
        }
    for (int c = 0; c < 16; c++) q->ch_slide[c] = 0.0f;
}

/* ---- the LFO's rate ---- */

/* Right of centre: in time with the Move, from 8 bars at the centre to 1/64
 * at the end, in beats. Left: free, from one cycle in 50 s to 20 Hz. */
static const float SYNC_BEATS[] = {
    32, 16, 8, 4, 3, 2, 4.0f / 3, 1.5f, 1, 2.0f / 3, 0.75f, 0.5f, 1.0f / 3, 0.375f, 0.25f, 1.0f / 6, 0.125f, 0.0625f,
};
#define NSYNC ((int)(sizeof(SYNC_BEATS) / sizeof(SYNC_BEATS[0])))

int quilt_rate_sync(float rate) {
    if (rate <= 0.0f) return -1;
    int i = (int)(rate * NSYNC);
    return i >= NSYNC ? NSYNC - 1 : i;
}
float quilt_rate_beats(int sync) { return SYNC_BEATS[sync]; }
float quilt_rate_hz(float rate) { return 0.02f * powf(1000.0f, fminf(1.0f, -fminf(rate, 0.0f))); }

static const host_api_v1_t *host;
void quilt_mod_set_host(const host_api_v1_t *h) { host = h; }

static float bpm(void) {
    float b = host && host->get_bpm ? host->get_bpm() : 120.0f;
    return b > 20.0f && b < 400.0f ? b : 120.0f;
}

/* ---- the sources ---- */

static float rnd(uint32_t *s) {
    *s ^= *s << 13, *s ^= *s >> 17, *s ^= *s << 5;
    return (float)(*s >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

static float lfo_shape(int shape, float p, float r0, float r1) {
    switch (shape) {
    case 1: return 1.0f - 4.0f * fabsf(p - 0.5f);                           /* triangle */
    case 2: return 1.0f - 2.0f * p;                                         /* saw, falling */
    case 3: return 2.0f * p - 1.0f;                                         /* ramp, rising */
    case 4: return p < 0.5f ? 1.0f : -1.0f;
    case 5: return r1;                                                      /* a new value each cycle */
    case 6: return r0 + (r1 - r0) * (0.5f - 0.5f * cosf(3.14159265f * p));  /* gliding between them */
    default: return fast_sin(p);
    }
}

static void lfo_advance(quilt_t *q, int m, int frames) {
    struct mod_run *r = &q->run[m];
    const float rate = r->e[MP_RATE];
    const int sync = quilt_rate_sync(rate);
    double ph;
    int locked = 0;
    const double beat = sync >= 0 && host && host->get_beat_position ? host->get_beat_position() : -1.0;
    if (sync >= 0 && beat >= 0.0) {
        ph = beat / SYNC_BEATS[sync];   /* locked to the bar */
        locked = 1;
    } else {
        const float hz = sync >= 0 ? bpm() / 60.0f / SYNC_BEATS[sync] : quilt_rate_hz(rate);
        ph = r->ph + (double)hz * frames / QUILT_SR;
    }
    if (floor(ph) != floor(r->ph)) { r->r0 = r->r1; r->r1 = rnd(&r->rng); }
    r->ph = locked ? ph : ph - floor(ph);
    const float p = (float)(ph - floor(ph));
    r->lfo = lfo_shape((int)r->e[MP_SHAPE], p, r->r0, r->r1);
}

static float seconds(float x) { return 0.002f * powf(5000.0f, x); }   /* 2 ms .. 10 s */

/* An envelope that rises while held and falls once let go. */
static float env_step(float e, int held, float rise, float fall, float dt) {
    if (held) return fminf(1.0f, e + dt / seconds(rise));
    return e * expf(-dt * 4.6f / seconds(fall));
}

static float lag_step(float s, float x, float lag, float dt) {
    const float tau = 0.3f * lag * lag;
    return tau < 1e-4f ? x : s + (x - s) * (1.0f - expf(-dt / tau));
}

static float mpe_raw(int axis, float press, float slide, float bend) {
    return axis == 1 ? slide : axis == 2 ? bend : press;
}

static int kind_of(const float *e) { return (int)e[MP_KIND]; }

/* ---- destinations ---- */

static float modulate(const param_def_t *d, float v, float off) {
    if (d->max <= d->min) return v;
    float u = (v - d->min) / (d->max - d->min) + off;
    u = u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
    float r = d->min + u * (d->max - d->min);
    return d->kind == PK_FLOAT ? r : floorf(r + 0.5f);
}

static int aim_mod(int aim, int *p) {
    if (aim < A_MODS || aim >= NAIMS) return -1;
    *p = (aim - A_MODS) % MP_COUNT;
    return (aim - A_MODS) / MP_COUNT;
}

/* Every route's offset from a context's sources, merged by destination.
 * src[m] is modulator m's value for this context. Routes into the
 * modulators themselves are left to mod_block. */
static void gather(const quilt_t *q, const float *src, mod_ctx_t *c) {
    c->n = 0;
    for (int m = 0; m < QUILT_MODS; m++)
        for (int r = 0; r < 2; r++) {
            const int aim = (int)q->run[m].e[MP_AIM1 + 2 * r];
            const float d = q->run[m].e[MP_DEPTH1 + 2 * r] * src[m];
            if (aim <= 0 || aim >= A_MODS || d == 0.0f) continue;
            int i = 0;
            while (i < c->n && c->aim[i] != aim) i++;
            if (i == c->n) { c->aim[c->n] = aim; c->off[c->n++] = 0.0f; }
            c->off[i] += d;
        }
}

void mod_apply(quilt_t *q, mod_ctx_t *c, int inst, float *gd) {
    memcpy(c->g, q->g, sizeof(c->g));
    memcpy(c->gs, q->gs, sizeof(c->gs));
    memcpy(c->gsp, q->gs_prev, sizeof(c->gsp));
    c->ns = 0;
    float dn[G_COUNT] = { 0 };
    const shape_t s = QUILT_INST[inst].shape;
    for (int i = 0; i < c->n; i++) {
        const int a = c->aim[i];
        float *p = NULL;
        const param_def_t *d = NULL;
        int g = a < A_WORDS ? MAIN_G[a] : (a >= A_FX && a < A_MODS ? FX_G[a - A_FX] : -1);
        if (g >= 0) { dn[g] = modulate(&QUILT_GLOBALS[g], q->g[g], c->off[i]) - q->g[g]; continue; }
        if (a == 3) { p = &q->charv[inst]; d = &CHAR_DEF; }
        else if (a >= A_WORDS && a < A_FX) {
            const int k = q->word_slot[s][a - A_WORDS];
            if (k < 0) continue;
            p = &q->slot[inst][k];
            d = quilt_shape_param(s, k);
        }
        if (!p) continue;
        c->ptr[c->ns] = p;
        c->val[c->ns++] = *p;
        *p = modulate(d, *p, c->off[i]);
    }
    for (int g = 0; g < G_COUNT; g++) {
        q->g[g] += dn[g];
        q->gs[g] += dn[g];
        q->gs_prev[g] += gd ? gd[g] : dn[g];
        if (gd) gd[g] = dn[g];
    }
}

void mod_restore(quilt_t *q, const mod_ctx_t *c) {
    for (int i = c->ns - 1; i >= 0; i--) *c->ptr[i] = c->val[i];
    memcpy(q->g, c->g, sizeof(c->g));
    memcpy(q->gs, c->gs, sizeof(c->gs));
    memcpy(q->gs_prev, c->gsp, sizeof(c->gsp));
}

/* ---- each block ---- */

void mod_block(quilt_t *q, int frames) {
    const float dt = (float)frames / QUILT_SR;
    const int held = q->held_keys > 0 || q->pedal;
    for (int m = 0; m < QUILT_MODS; m++) {
        struct mod_run *r = &q->run[m];
        /* This modulator's knobs, moved by the others as they stand. */
        float off[MP_COUNT] = { 0 };
        for (int j = 0; j < QUILT_MODS; j++)
            for (int k = 0; k < 2; k++) {
                int p;
                if (aim_mod((int)q->run[j].e[MP_AIM1 + 2 * k], &p) == m)
                    off[p] += q->run[j].e[MP_DEPTH1 + 2 * k] * q->run[j].out;
            }
        for (int p = 0; p < MP_COUNT; p++) r->e[p] = modulate(&QUILT_MOD_PARAMS[p], q->mod[m][p], off[p]);
        switch (kind_of(r->e)) {
        case MK_LFO: lfo_advance(q, m, frames); r->out = r->lfo; break;
        case MK_ENV: r->env = env_step(r->env, held, r->e[MP_RISE], r->e[MP_FALL], dt); r->out = r->env; break;
        case MK_MPE:
            r->mpe = lag_step(r->mpe, mpe_raw((int)r->e[MP_AXIS], q->lead_press, q->lead_slide, q->lead_bend),
                              r->e[MP_LAG], dt);
            r->out = r->mpe;
            break;
        default: r->out = q->lead_vel; break;
        }
    }
}

/* A note's own sources, one block on. */
static void voice_sources(const quilt_t *q, voice_t *v, int frames, float *src) {
    const float dt = (float)frames / QUILT_SR;
    voice_mod_t *vm = &v->mod;
    for (int m = 0; m < QUILT_MODS; m++) {
        const float *e = q->run[m].e;
        switch (kind_of(e)) {
        case MK_LFO: src[m] = q->run[m].lfo; break;
        case MK_ENV: vm->env[m] = env_step(vm->env[m], v->held || q->pedal, e[MP_RISE], e[MP_FALL], dt); src[m] = vm->env[m]; break;
        case MK_MPE:
            vm->mpe[m] = lag_step(vm->mpe[m], mpe_raw((int)e[MP_AXIS], v->press, vm->slide, vm->bend), e[MP_LAG], dt);
            src[m] = vm->mpe[m];
            break;
        default: src[m] = v->vel; break;
        }
    }
}

void mod_voice(quilt_t *q, voice_t *v, int frames, mod_ctx_t *c) {
    float src[QUILT_MODS];
    voice_sources(q, v, frames, src);
    gather(q, src, c);
}

void mod_shared(quilt_t *q, mod_ctx_t *c) {
    float src[QUILT_MODS];
    for (int m = 0; m < QUILT_MODS; m++) src[m] = q->run[m].out;
    gather(q, src, c);
}

/* A new note: its sources as they start, and which instrument plays it. */
int mod_note(quilt_t *q, int note, int vel, mod_ctx_t *c, voice_mod_t *vm) {
    const int ch = q->midi_chan & 15;
    q->lead_vel = (float)vel / 127.0f;
    q->lead_note = note;
    q->lead_chan = ch;
    q->lead_press = 0.0f;
    q->lead_slide = q->ch_slide[ch];
    q->lead_bend = q->ch_bend[ch];
    memset(vm, 0, sizeof(*vm));
    vm->chan = ch;
    vm->slide = q->ch_slide[ch];
    vm->bend = q->ch_bend[ch];
    float src[QUILT_MODS];
    for (int m = 0; m < QUILT_MODS; m++) {
        const float *e = q->run[m].e;
        switch (kind_of(e)) {
        case MK_LFO: src[m] = q->run[m].lfo; break;
        case MK_ENV: vm->env[m] = seconds(e[MP_RISE]) < 0.003f ? 1.0f : 0.0f; src[m] = vm->env[m]; break;
        case MK_MPE: vm->mpe[m] = mpe_raw((int)e[MP_AXIS], 0.0f, vm->slide, vm->bend); src[m] = vm->mpe[m]; break;
        default: src[m] = q->lead_vel; break;
        }
    }
    float type_off = 0.0f;
    for (int m = 0; m < QUILT_MODS; m++)
        for (int r = 0; r < 2; r++)
            if ((int)q->run[m].e[MP_AIM1 + 2 * r] == 1) type_off += q->run[m].e[MP_DEPTH1 + 2 * r] * src[m];
    gather(q, src, c);
    if (type_off == 0.0f) return q->type;
    const float t = modulate(&QUILT_TYPE_PARAM, (float)quilt_type_index(q->type), type_off);
    return quilt_type_inst((int)t);
}

/* ---- MPE input ---- */

void quilt_slide(quilt_t *q, int ch, float x) {
    q->ch_slide[ch & 15] = x;
    if ((ch & 15) == q->lead_chan) q->lead_slide = x;
    for (int i = 0; i < QUILT_VOICES + QUILT_GHOSTS; i++)
        if (q->v[i].active && q->v[i].mod.chan == (ch & 15)) q->v[i].mod.slide = x;
}

void quilt_bend(quilt_t *q, int ch, float x) {
    q->ch_bend[ch & 15] = x;
    if ((ch & 15) == q->lead_chan) q->lead_bend = x;
    for (int i = 0; i < QUILT_VOICES + QUILT_GHOSTS; i++)
        if (q->v[i].active && q->v[i].mod.chan == (ch & 15)) q->v[i].mod.bend = x;
}
