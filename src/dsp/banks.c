/*
 * The banks engine: always-running generators that keys only open. DESIGN.md,
 * Banks.
 *
 *   Tonewheel Organ  91 sine wheels on Hammond's gear ratios, run once for
 *                    every key. Each key's nine drawbar contacts connect
 *                    nine wheels to the busbars, closing a fraction of a
 *                    millisecond apart: the key click. Percussion on the
 *                    third harmonic, single-triggered. Leakage from the
 *                    neighbouring wheels. The scanner vibrato and chorus
 *                    (LUSH) on the sum, then the Leslie (fx.c).
 *   Flute Organ      Pipe ranks at 8', 4' and 2', one phase per pitch shared
 *                    by every rank that sounds it. The 8' is stopped (odd
 *                    partials), EDGE opens it. A pipe speaks with its upper
 *                    partials first and a breathy chiff (PUFF), as
 *                    Castellengo measured.
 *   String Ensemble  Twelve top-octave sawtooth oscillators divided down, so
 *                    every octave is locked in phase, as a string machine's
 *                    are. 8', 4' and 2' registers, a low-pass (SOFT) and a
 *                    presence peak (EDGE) on the sum, then the ensemble
 *                    (LUSH): three delays swept by a slow and a fast LFO at
 *                    120 degrees, after Raffel & Smith.
 *
 * The organ costs the same with one key or thirty: the wheels run once, and
 * a settled key is only nine gains on the busbars, ramped per block. A key
 * in its first few milliseconds is mixed per sample, so its contacts close
 * when they should.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define TWO_PI 6.28318530718f

typedef enum { BK_NONE, BK_WHEELS, BK_PIPES, BK_STRINGS } bank_kind_t;

static bank_kind_t kind_of(int inst) {
    const char *n = QUILT_INST[inst].name;
    if (!strcmp(n, "Tonewheel Organ")) return BK_WHEELS;
    if (!strcmp(n, "Flute Organ")) return BK_PIPES;
    if (!strcmp(n, "String Ensemble")) return BK_STRINGS;
    return BK_NONE;
}

int banks_supports(int inst) { return kind_of(inst) != BK_NONE; }

static float slot(const quilt_t *q, int inst, const char *key, float fallback) {
    int i = quilt_shape_key_in(QUILT_INST[inst].shape, key);
    return i < 0 ? fallback : q->slot[inst][i];
}

static uint32_t xorshift(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static float rnd(uint32_t *s) { return (float)(xorshift(s) >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */

/* ---- the wheels ---- */

/* Hammond's twelve gear ratios: wheel frequency = 20 rev/s x ratio x teeth,
 * the teeth doubling each octave. Close to equal temperament, not on it. */
static const float GEAR[12] = {
    85.0f / 104.0f, 71.0f / 82.0f, 67.0f / 73.0f, 105.0f / 108.0f, 103.0f / 100.0f, 84.0f / 77.0f,
    74.0f / 64.0f, 98.0f / 80.0f, 96.0f / 74.0f, 88.0f / 64.0f, 67.0f / 46.0f, 108.0f / 70.0f,
};

static float wheel_hz(int w) { return 20.0f * GEAR[w % 12] * (float)(2 << (w / 12)); }

/* The drawbars, in the order of their keys: 16', 5 1/3', 8', 4', 2 2/3', 2',
 * 1 3/5', 1 1/3', 1'. Semitones above the 8' wheel, and harmonic number. */
static const char *const BAR_KEY[9] = { "o_16", "o_513", "o_8", "o_4", "o_223", "o_2", "o_135", "o_113", "o_1" };
static const int BAR_SEMI[9] = { -12, 7, 0, 12, 19, 24, 28, 31, 36 };
static const float BAR_HARM[9] = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };

/* The wheel a drawbar takes for a key: the 8' of C2 is wheel 13 (index 12).
 * Past either end the wheels fold back an octave, as on the instrument. */
static int wheel_for(int note, int bar) {
    int w = note - 36 + 12 + BAR_SEMI[bar];
    while (w < 0) w += 12;
    while (w > 90) w -= 12;
    return w;
}

void banks_reset(banks_t *b) {
    memset(b, 0, sizeof(*b));
    uint32_t s = 0x2545F491u;
    for (int w = 0; w < BANK_WHEELS; w++) {
        /* Every wheel at its own phase: they were never in step. */
        float ph = rnd(&s) * TWO_PI, f = w < 91 ? wheel_hz(w) : 0.0f;
        b->wr[w / 4][w % 4] = w < 91 ? cosf(ph) : 0.0f;
        b->wi[w / 4][w % 4] = w < 91 ? sinf(ph) : 0.0f;
        b->wc[w / 4][w % 4] = cosf(TWO_PI * f / QUILT_SR);
        b->ws[w / 4][w % 4] = sinf(TWO_PI * f / QUILT_SR);
    }
    for (int p = 0; p < 128; p++) b->phase[p] = rnd(&s);
    for (int k = 0; k < BANK_KEYS; k++) b->key[k].rng = 0x9E3779B9u ^ (uint32_t)(k * 2654435761u);
}

int banks_active(const quilt_t *q) {
    for (int k = 0; k < BANK_KEYS; k++)
        if (q->banks.key[k].on) return 1;
    return 0;
}

static int any_held(const banks_t *b, int inst) {
    for (int k = 0; k < BANK_KEYS; k++)
        if (b->key[k].on && b->key[k].held && b->key[k].inst == inst) return 1;
    return 0;
}

void banks_note_on(quilt_t *q, int note, int vel) {
    (void)vel;   /* organs and string machines have no touch */
    banks_t *b = &q->banks;
    bank_key_t *k = &b->key[note & 127];
    const int inst = q->type;
    if (kind_of(inst) == BK_WHEELS) {
        /* Percussion fires only when no key is down, and then for every key
         * struck with it. */
        if (!any_held(b, inst)) { b->perc = 1.0f; b->perc_fresh = 1; }
        k->perc = b->perc_fresh;
        for (int d = 0; d < 9; d++) k->contact[d] = rnd(&k->rng) * 0.0012f;
        b->click += 1.0f;
    }
    if (!k->on) { k->att = k->att_up = 0.0f; k->rel = 1.0f; }
    k->on = 1;
    k->held = 1;
    k->inst = inst;
    k->t = 0.0f;
    /* Struck again while it fades, a key keeps its level (k->rel) and comes
     * back up from where it is, not in one step: the pipes and strings rise
     * in about 3 ms, the wheels across a block. */
    k->chiff = 1.0f;
    k->chiff_up = 0.0f;
}

void banks_note_off(quilt_t *q, int note) {
    bank_key_t *k = &q->banks.key[note & 127];
    if (!k->on || !k->held) return;
    k->held = 0;
    if (kind_of(k->inst) == BK_WHEELS) q->banks.click += 0.5f;   /* the contacts opening */
}

/* Drawbar n of 8 to a gain: each step is 3 dB, as on the instrument. */
static float bar_gain(float n) { return n < 0.5f ? 0.0f : powf(10.0f, -(8.0f - n) * 0.15f); }

/* The release: a fall of 60 dB in a time set by DECAY. */
static float release_t60(const quilt_t *q, bank_kind_t kind) {
    const float d = q->g[G_DECAY];
    return kind == BK_WHEELS ? 0.01f * powf(200.0f, d) : 0.05f * powf(40.0f, d);
}

static void wheels(quilt_t *q, int inst, float *mono, int frames) {
    banks_t *b = &q->banks;
    const float dt = (float)frames / QUILT_SR;
    /* Run the wheels for this block. The lowest twelve are not quite sines. */
    for (int n = 0; n < frames; n++) {
        for (int g = 0; g < BANK_WHEELS / 4; g++) {
            v4 t = b->wc[g] * b->wr[g] - b->ws[g] * b->wi[g];
            b->wi[g] = b->ws[g] * b->wr[g] + b->wc[g] * b->wi[g];
            b->wr[g] = t;
            for (int i = 0; i < 4; i++) b->wheel[n][g * 4 + i] = b->wi[g][i];
        }
        for (int w = 0; w < 12; w++) {
            float y = b->wheel[n][w];
            b->wheel[n][w] = y + 0.04f * (3.0f * y - 4.0f * y * y * y);
        }
    }
    for (int g = 0; g < BANK_WHEELS / 4; g++) {
        v4 m = (v4){ 1.5f, 1.5f, 1.5f, 1.5f } - 0.5f * (b->wr[g] * b->wr[g] + b->wi[g] * b->wi[g]);
        b->wr[g] *= m;
        b->wi[g] *= m;
    }

    /* SOFT: the drawbars above 8' fall away, 13 dB at the 4' and twice
     * that at the 2' end to end, at the same overall level; at the middle
     * the registration is literal. */
    float lvl[9], lit = 0.0f, sof = 0.0f;
    const float soft = q->g[G_SOFT];
    for (int d = 0; d < 9; d++) {
        const float g = bar_gain(slot(q, inst, BAR_KEY[d], 0.0f));
        lvl[d] = g * powf(fmaxf(BAR_HARM[d], 1.0f), -(soft - 0.5f) * 4.4f);
        lit += g * g;
        sof += lvl[d] * lvl[d];
    }
    if (sof > 0.0f)
        for (int d = 0; d < 9; d++) lvl[d] *= sqrtf(lit / sof);
    const float ping = slot(q, inst, "o_ping", 0.0f) * 1.4f;
    const float perc_k = expf(-6.91f * dt / (0.25f * powf(10.0f, slot(q, inst, "o_tail", 0.3f))));
    const float perc0 = b->perc, perc1 = b->perc * perc_k;
    const float ramp = 0.006f * powf(0.01f, slot(q, inst, "o_click", 0.3f));
    const float att_len = 0.0012f + ramp;
    const float rel_k = expf(-6.91f * dt / release_t60(q, BK_WHEELS));

    float g0[BANK_WHEELS], g1[BANK_WHEELS];
    memset(g0, 0, sizeof(g0));
    memset(g1, 0, sizeof(g1));
    memset(mono, 0, sizeof(float) * (size_t)frames);
    for (int note = 0; note < BANK_KEYS; note++) {
        bank_key_t *k = &b->key[note];
        if (!k->on || k->inst != inst) continue;
        const float r0 = k->rel, r1 = k->sus ? 1.0f : k->rel * rel_k;
        const float p0 = k->perc ? ping * perc0 : 0.0f, p1 = k->perc ? ping * perc1 : 0.0f;
        const int wp = wheel_for(note, 4);   /* the percussion: the third harmonic */
        if (k->t < att_len) {
            /* Its contacts are still closing: mixed sample by sample. */
            for (int n = 0; n < frames; n++) {
                const float t = k->t + (float)n / QUILT_SR, a = (float)(n + 1) / (float)frames;
                const float r = r0 + (r1 - r0) * a;
                float y = 0.0f;
                for (int d = 0; d < 9; d++) {
                    if (lvl[d] <= 0.0f) continue;
                    float c = (t - k->contact[d]) / ramp;
                    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
                    y += lvl[d] * c * b->wheel[n][wheel_for(note, d)];
                }
                y += (p0 + (p1 - p0) * a) * b->wheel[n][wp];
                mono[n] += y * r;
            }
        } else {
            for (int d = 0; d < 9; d++) {
                int w = wheel_for(note, d);
                g0[w] += lvl[d] * r0;
                g1[w] += lvl[d] * r1;
            }
            g0[wp] += p0 * r0;
            g1[wp] += p1 * r1;
        }
        k->t += dt;
        k->rel = r1;
        if (!k->sus && k->rel < 1e-4f) k->on = 0;
    }
    b->perc = perc1;
    b->perc_fresh = 0;

    /* LEAK: each sounding wheel bleeds into its neighbours, and its partner
     * four octaves away on the same shaft. */
    const float leak = slot(q, inst, "o_leak", 0.2f) * 0.06f;
    float l0[BANK_WHEELS], l1[BANK_WHEELS];
    for (int w = 0; w < BANK_WHEELS; w++) {
        float a0 = 0.0f, a1 = 0.0f;
        if (w > 0) { a0 += g0[w - 1]; a1 += g1[w - 1]; }
        if (w < 90) { a0 += g0[w + 1]; a1 += g1[w + 1]; }
        if (w >= 48) { a0 += g0[w - 48]; a1 += g1[w - 48]; }
        if (w + 48 < 91) { a0 += g0[w + 48]; a1 += g1[w + 48]; }
        l0[w] = w < 91 ? g0[w] + leak * a0 : 0.0f;
        l1[w] = w < 91 ? g1[w] + leak * a1 : 0.0f;
    }
    v4 gv[BANK_WHEELS / 4], gs[BANK_WHEELS / 4];
    const float inv = 1.0f / (float)frames;
    for (int g = 0; g < BANK_WHEELS / 4; g++)
        for (int i = 0; i < 4; i++) {
            gv[g][i] = l0[g * 4 + i];
            gs[g][i] = (l1[g * 4 + i] - l0[g * 4 + i]) * inv;
        }
    for (int n = 0; n < frames; n++) {
        v4 acc = { 0, 0, 0, 0 };
        const v4 *wv = (const v4 *)b->wheel[n];
        for (int g = 0; g < BANK_WHEELS / 4; g++) {
            gv[g] += gs[g];
            acc += gv[g] * wv[g];
        }
        mono[n] += acc[0] + acc[1] + acc[2] + acc[3];
    }

    /* CLICK: the contacts' bounce, a short burst about 2-4 kHz on each key
     * down and up; SOFT takes the edge off it. */
    const float click = slot(q, inst, "o_click", 0.3f) * (1.2f - soft) * 0.5f;
    const float cd = expf(-1.0f / (0.004f * QUILT_SR));
    for (int n = 0; n < frames && b->click > 1e-4f; n++) {
        float x = rnd(&b->key[0].rng) * 2.0f - 1.0f;
        b->click_lp += (x - b->click_lp) * 0.45f;
        b->click_bp += ((x - b->click_lp) - b->click_bp) * 0.6f;
        mono[n] += b->click_bp * b->click * click;
        b->click *= cd;
    }
    if (b->click > 4.0f) b->click = 4.0f;

    /* LUSH: the scanner. A delay swept by a triangle at 6.9 Hz, up to
     * 0.5 ms either way, mixed half and half with the dry sound at full,
     * which is C3; turned down it is gentler, and at zero it is off. */
    const float lush = q->charv[inst];
    if (lush > 0.001f) {
        const float depth = lush * 0.0005f * QUILT_SR, centre = 0.0012f * QUILT_SR;
        const float inc = 6.9f / QUILT_SR, lk = 1.0f - expf(-TWO_PI * 7000.0f / QUILT_SR);
        for (int n = 0; n < frames; n++) {
            b->scan[b->scan_w & 511] = mono[n];
            b->scan_ph += inc;
            if (b->scan_ph >= 1.0f) b->scan_ph -= 1.0f;
            float tri = b->scan_ph < 0.5f ? 4.0f * b->scan_ph - 1.0f : 3.0f - 4.0f * b->scan_ph;
            float d = centre + depth * tri, fl = floorf(d), fr = d - fl;
            int i0 = b->scan_w - (int)fl;
            float y = b->scan[i0 & 511] * (1.0f - fr) + b->scan[(i0 - 1) & 511] * fr;
            b->scan_lp += (y - b->scan_lp) * lk;
            b->scan_w++;
            mono[n] = mono[n] * (1.0f - 0.5f * lush) + b->scan_lp * 0.5f * lush;
        }
    }
}

/* ---- pipes and strings: keys reading shared generators ---- */

#define RISE_K 0.0075f          /* back up in about 3 ms */

static float midi_hz(int p) { return 440.0f * powf(2.0f, (float)(p - 69) / 12.0f); }

/* The register a knob sets: zero is silent, full is full, a smooth taper between. */
static float rank(float v) { return v * v; }

static void pipes(quilt_t *q, int inst, float *mono, int frames) {
    banks_t *b = &q->banks;
    const float dt = 1.0f / QUILT_SR;
    const float soft = q->g[G_SOFT], edge = slot(q, inst, "k_edge", 0.4f);
    const float puff = q->charv[inst];
    const float lv[3] = { rank(slot(q, inst, "k_8", 0.8f)), rank(slot(q, inst, "k_4", 0.4f)),
                          rank(slot(q, inst, "k_2", 0.0f)) };
    /* SOFT is the wind: each partial above the first 3 dB per step softer at
     * full, as much louder at zero; and a gentler wind speaks more slowly. */
    const float s = powf(10.0f, -(soft - 0.5f) * 0.7f);
    /* The 8' is stopped, odd partials only; EDGE opens it. The 4' and 2' are open. */
    float h[3][5] = {
        { 1.0f, 0.3f * edge, 0.2f + 0.15f * edge, 0.1f * edge, 0.06f + 0.06f * edge },
        { 1.0f, 0.25f + 0.2f * edge, 0.1f + 0.1f * edge, 0.04f + 0.06f * edge, 0.02f * edge },
        { 1.0f, 0.12f + 0.15f * edge, 0.04f * edge, 0.0f, 0.0f },
    };
    for (int r = 0; r < 3; r++)
        for (int k = 1; k < 5; k++) h[r][k] *= powf(s, (float)k);
    const float speech = 0.015f * powf(27.0f, slot(q, inst, "k_swell", 0.4f)) * (0.7f + 0.6f * soft);
    const float ak = 1.0f - expf(-dt / (speech * 0.4f)), auk = 1.0f - expf(-dt / (speech * 0.15f));
    /* The chiff rises in 1.5 ms, so a pipe never speaks with a click. */
    const float ck = expf(-dt / 0.03f), cuk = 1.0f - expf(-dt / 0.0015f);
    const float rel_k = expf(-6.91f * dt / release_t60(q, BK_PIPES));

    /* This block's keys and the pitches they need, worked out once. */
    int keys[BANK_KEYS], nk = 0, pitches[128], np = 0, need[128];
    memset(need, 0, sizeof(need));
    for (int note = 0; note < BANK_KEYS; note++) {
        const bank_key_t *k = &b->key[note];
        if (!k->on || k->inst != inst) continue;
        keys[nk++] = note;
        for (int r = 0; r < 3; r++) {
            const int p = note + 12 * r;
            if (lv[r] > 0.0f && p < 128 && !need[p]) { need[p] = 1; pitches[np++] = p; }
        }
    }
    float inc[128], sn[128], cs[128];
    int has4[128], has5[128];
    for (int i = 0; i < np; i++) {
        const int p = pitches[i];
        /* Each pipe a hair off true, as real ones are. */
        const float cents = (float)((p * 7919) % 13 - 6) * 0.15f, hz = midi_hz(p);
        inc[p] = hz * powf(2.0f, cents / 1200.0f) / QUILT_SR;
        has4[p] = 4.0f * hz < 0.45f * QUILT_SR;
        has5[p] = 5.0f * hz < 0.45f * QUILT_SR;
    }
    /* The chiff's resonator, about the fourth partial of each key. */
    float ra1[BANK_KEYS], ra2 = 0.97f * 0.97f;
    for (int i = 0; i < nk; i++) {
        const float f = fminf(4.0f * midi_hz(keys[i]), 0.4f * QUILT_SR);
        ra1[i] = 2.0f * 0.97f * cosf(TWO_PI * f / QUILT_SR);
    }
    const float ob_g = puff * 0.8f * (1.2f - soft), nz_g = puff * 1.2f * (1.2f - soft) * 0.03f;

    memset(mono, 0, sizeof(float) * (size_t)frames);
    for (int n = 0; n < frames; n++) {
        for (int i = 0; i < np; i++) {
            const int p = pitches[i];
            b->phase[p] += inc[p];
            if (b->phase[p] >= 1.0f) b->phase[p] -= 1.0f;
            sn[p] = fast_sin(b->phase[p]);
            cs[p] = fast_sin(b->phase[p] + 0.25f);
        }
        for (int i = 0; i < nk; i++) {
            bank_key_t *k = &b->key[keys[i]];
            const int note = keys[i];
            k->att += (1.0f - k->att) * ak;
            k->att_up += (1.0f - k->att_up) * auk;
            k->chiff_up += (1.0f - k->chiff_up) * cuk;
            const float chiff = k->chiff * k->chiff_up;
            float y = 0.0f;
            for (int r = 0; r < 3; r++) {
                const int p = note + 12 * r;
                if (lv[r] <= 0.0f || p >= 128) continue;
                const float x = sn[p], c = cs[p], x2 = x * x;
                const float s2 = 2.0f * x * c, s3 = x * (3.0f - 4.0f * x2);
                float up = h[r][1] * s2 + h[r][2] * s3;
                if (has4[p]) up += h[r][3] * 2.0f * s2 * (1.0f - 2.0f * x2);
                if (has5[p]) up += h[r][4] * x * (5.0f + x2 * (16.0f * x2 - 20.0f));
                /* PUFF: the pipe overblows for an instant (a stopped pipe to
                 * its twelfth, an open one to its octave). */
                const float ob = (r == 0 ? s3 : s2) * chiff * ob_g;
                y += lv[r] * (k->att * h[r][0] * x + k->att_up * up + ob);
            }
            /* ... and a breath of noise rings in the pipe's mouth. */
            if (nz_g > 0.0f && k->chiff > 1e-4f) {
                const float nz = rnd(&k->rng) * 2.0f - 1.0f;
                const float o = ra1[i] * k->bp[0] - ra2 * k->bp[1] + nz;
                k->bp[1] = k->bp[0];
                k->bp[0] = o;
                y += o * chiff * nz_g;
            }
            k->chiff *= ck;
            if (!k->sus) k->rel *= rel_k;
            else if (k->rel < 1.0f) k->rel = fminf(1.0f, k->rel + (1.0f - k->rel) * RISE_K + 1e-6f);
            mono[n] += y * k->rel;
        }
    }
    for (int i = 0; i < nk; i++) {
        bank_key_t *k = &b->key[keys[i]];
        k->t += (float)frames * dt;
        if (!k->sus && k->rel < 1e-4f) k->on = 0;
    }
}

/* A sawtooth's step, smoothed over one sample either side (PolyBLEP). */
static float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static void strings(quilt_t *q, int inst, float *left, float *right, int frames) {
    banks_t *b = &q->banks;
    const float dt = 1.0f / QUILT_SR;
    const float lv[3] = { rank(slot(q, inst, "k_8", 0.8f)), rank(slot(q, inst, "k_4", 0.4f)),
                          rank(slot(q, inst, "k_2", 0.0f)) };
    const float swell = 0.02f * powf(60.0f, slot(q, inst, "k_swell", 0.4f));
    const float ak = 1.0f - expf(-dt / (swell * 0.4f));
    const float rel_k = expf(-6.91f * dt / release_t60(q, BK_STRINGS));
    /* SWAY: a vibrato, up to 20 cents, at SPEED. */
    const float vib_hz = 0.5f + 7.5f * q->gs[G_SPEED];
    b->vib_ph += vib_hz * (float)frames / QUILT_SR;
    if (b->vib_ph >= 1.0f) b->vib_ph -= 1.0f;
    const float vib = 1.0f + 0.012f * q->gs[G_SWAY] * fast_sin(b->vib_ph);
    float top_inc[12];
    for (int pc = 0; pc < 12; pc++) top_inc[pc] = midi_hz(108 + pc) * vib / QUILT_SR;

    /* SOFT: a low-pass from 12 kHz down to 400 Hz, two poles. EDGE: a
     * presence peak about 2.2 kHz, up to +9.5 dB. */
    const float fc = 12000.0f * powf(400.0f / 12000.0f, q->g[G_SOFT]);
    const float lk = 1.0f - expf(-TWO_PI * fc / QUILT_SR);
    const float edge = slot(q, inst, "k_edge", 0.4f) * 2.0f;
    const float pf = 2.0f * sinf(3.14159265f * 2200.0f / QUILT_SR), pq = 1.0f / 1.5f;

    const float lush = q->charv[inst];
    const float slow_inc = 0.63f / QUILT_SR, fast_inc = 6.3f / QUILT_SR;
    int keys[BANK_KEYS], nk = 0;
    for (int note = 0; note < BANK_KEYS; note++)
        if (b->key[note].on && b->key[note].inst == inst) keys[nk++] = note;
    for (int n = 0; n < frames; n++) {
        for (int pc = 0; pc < 12; pc++) {
            b->top_ph[pc] += top_inc[pc];
            if (b->top_ph[pc] >= 1.0f) { b->top_ph[pc] -= 1.0f; b->top_count[pc]++; }
        }
        float x = 0.0f;
        for (int i = 0; i < nk; i++) {
            const int note = keys[i];
            bank_key_t *k = &b->key[note];
            const int pc = note % 12;
            int oct = (108 + pc - note) / 12;
            if (oct < 0) oct = 0;
            k->att += (1.0f - k->att) * ak;
            float y = 0.0f;
            for (int r = 0; r < 3; r++) {
                int dv = oct - r;
                if (lv[r] <= 0.0f || dv < 0) continue;
                /* Divided down from the top octave: locked in phase. */
                const float div = (float)(1u << dv);
                const float ph = ((float)(b->top_count[pc] & ((1u << dv) - 1u)) + b->top_ph[pc]) / div;
                const float inc = top_inc[pc] / div;
                y += lv[r] * (2.0f * ph - 1.0f - blep(ph, inc));
            }
            if (!k->sus) k->rel *= rel_k;
            else if (k->rel < 1.0f) k->rel = fminf(1.0f, k->rel + (1.0f - k->rel) * RISE_K + 1e-6f);
            x += y * k->att * k->rel;
        }
        b->lp_z[0] += (x - b->lp_z[0]) * lk;
        b->lp_z[1] += (b->lp_z[0] - b->lp_z[1]) * lk;
        float lo = b->lp_z[1];
        /* A state-variable band-pass for the presence. */
        float hp = lo - b->bp_z[1] - pq * b->bp_z[0];
        b->bp_z[0] += pf * hp;
        b->bp_z[1] += pf * b->bp_z[0];
        float y = lo + edge * b->bp_z[0];

        /* LUSH: three taps on one delay line, each swept by both LFOs at its
         * own third of the cycle. The left hears the first two, the right
         * the last two. */
        float l = y, rgt = y;
        b->ens[b->ens_w & 1023] = y;
        b->ens_slow += slow_inc;
        if (b->ens_slow >= 1.0f) b->ens_slow -= 1.0f;
        b->ens_fast += fast_inc;
        if (b->ens_fast >= 1.0f) b->ens_fast -= 1.0f;
        if (lush > 0.001f) {
            float tap[3];
            for (int i = 0; i < 3; i++) {
                const float o = (float)i / 3.0f;
                const float d = (0.0055f + lush * (0.0016f * fast_sin(b->ens_slow + o) +
                                                   0.00025f * fast_sin(b->ens_fast + o))) * QUILT_SR;
                const float fl = floorf(d), fr = d - fl;
                const int i0 = b->ens_w - (int)fl;
                tap[i] = b->ens[i0 & 1023] * (1.0f - fr) + b->ens[(i0 - 1) & 1023] * fr;
            }
            l = y * (1.0f - lush) + lush * 0.6f * (tap[0] + tap[1]);
            rgt = y * (1.0f - lush) + lush * 0.6f * (tap[1] + tap[2]);
        }
        b->ens_w++;
        left[n] += l;
        right[n] += rgt;
    }
    for (int i = 0; i < nk; i++) {
        bank_key_t *k = &b->key[keys[i]];
        k->t += (float)frames * dt;
        if (!k->sus && k->rel < 1e-4f) k->on = 0;
    }
}

/* Loudness, matched to the other instruments (DESIGN.md, They are equally loud). */
static float bank_gain(bank_kind_t k) {
    return k == BK_WHEELS ? 0.0536f : (k == BK_PIPES ? 0.0635f : 0.0985f);
}

void banks_render(quilt_t *q, float *left, float *right, int frames) {
    banks_t *b = &q->banks;
    /* A key belongs to the instrument it was struck on; turning TYPE lets go
     * of it, and the pedal holds it as it would a struck note. */
    int present[3] = { 0, 0, 0 };
    for (int note = 0; note < BANK_KEYS; note++) {
        bank_key_t *k = &b->key[note];
        if (!k->on) continue;
        /* Caught by the pedal only if not already letting go. */
        k->sus = k->inst == q->type && (k->held || (q->pedal && k->rel >= 1.0f));
        present[kind_of(k->inst) - 1] = 1;
    }
    float mono[QUILT_MAX_BLOCK], l[QUILT_MAX_BLOCK], r[QUILT_MAX_BLOCK];
    for (int kind = BK_WHEELS; kind <= BK_STRINGS; kind++) {
        if (!present[kind - 1]) continue;
        int inst = -1;
        for (int note = 0; note < BANK_KEYS && inst < 0; note++)
            if (b->key[note].on && kind_of(b->key[note].inst) == (bank_kind_t)kind) inst = b->key[note].inst;
        const float g = bank_gain((bank_kind_t)kind);
        if (kind == BK_STRINGS) {
            memset(l, 0, sizeof(float) * (size_t)frames);
            memset(r, 0, sizeof(float) * (size_t)frames);
            strings(q, inst, l, r, frames);
            for (int n = 0; n < frames; n++) { left[n] += l[n] * g; right[n] += r[n] * g; }
        } else {
            if (kind == BK_WHEELS) wheels(q, inst, mono, frames);
            else pipes(q, inst, mono, frames);
            for (int n = 0; n < frames; n++) { left[n] += mono[n] * g; right[n] += mono[n] * g; }
        }
    }
}
