/*
 * The formant voice: the Choir. DESIGN.md, Synthetic.
 *
 * Each singer is a glottal pulse: a band-limited sum of harmonics falling
 * away geometrically (Moorer's closed form, so no harmonic ever folds
 * back), its slope set by how gently the note is sung. A soft voice's
 * folds close gently and its upper harmonics fall fast; a firm one's snap
 * shut. Breath noise rides on the pulse, strongest while the folds are open.
 *
 * The singers of a note go through a Klatt cascade of five formant
 * resonators, one cascade for each ear: the first singer into both, the
 * second into the left, the third into the right, and each ear's vocal
 * tract a little longer or shorter than the other's. So a crowd of three is
 * a section, wide and a little unalike, and one singer sits in the middle.
 * The formants come from the singers' own tables (bass, tenor, alto,
 * soprano) for the note's register, and VOWEL morphs oo -> oh -> ah. A
 * soprano raises her first formant to the note when it sings above it.
 *
 * Breath is pad pressure, or an automatic swell (PRESS), as for the winds.
 * Each singer has its own detune (SPLIT), its own slow drift, and its own
 * vibrato a little apart from the others' (SWAY, at SPEED).
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f

int choir_supports(int inst) { return !strcmp(QUILT_INST[inst].name, "Choir"); }

/* Keys outside the singers' range sound an octave in. */
#define LOWEST 36
#define HIGHEST 84

float choir_freq(int inst, int note) {
    (void)inst;
    while (note < LOWEST) note += 12;
    while (note > HIGHEST) note -= 12;
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* Formant frequencies and bandwidths, Hz: bass, tenor, alto, soprano, each
 * singing oo, oh, ah (the classic table, after Peterson & Barney's
 * measurements as tabulated for formant synthesis). */
static const float FORMANT[4][3][2][5] = {
    { { { 350, 600, 2400, 2675, 2950 }, { 40, 80, 100, 120, 120 } },
      { { 400, 750, 2400, 2600, 2900 }, { 40, 80, 100, 120, 120 } },
      { { 600, 1040, 2250, 2450, 2750 }, { 60, 70, 110, 120, 130 } } },
    { { { 350, 600, 2700, 2900, 3300 }, { 40, 60, 100, 120, 120 } },
      { { 400, 800, 2600, 2800, 3000 }, { 40, 80, 100, 120, 120 } },
      { { 650, 1080, 2650, 2900, 3250 }, { 80, 90, 120, 130, 140 } } },
    { { { 325, 700, 2530, 3500, 4950 }, { 50, 60, 170, 180, 200 } },
      { { 450, 800, 2830, 3500, 4950 }, { 70, 80, 100, 130, 135 } },
      { { 800, 1150, 2800, 3500, 4950 }, { 80, 90, 120, 130, 140 } } },
    { { { 325, 700, 2700, 3800, 4950 }, { 50, 60, 170, 180, 200 } },
      { { 450, 800, 2830, 3800, 4950 }, { 70, 80, 100, 130, 135 } },
      { { 800, 1150, 2900, 3900, 4950 }, { 80, 90, 120, 130, 140 } } },
};

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

static float noise(uint32_t *s) { return (float)(int32_t)xorshift(s) * (1.0f / 2147483648.0f); }

/* The five formants for this register and vowel: the register moves from
 * bass (below G2) to soprano (above G5) between the four tables. */
static void formants(float f0, float vowel, float F[5], float BW[5]) {
    float reg = fminf(3.0f, fmaxf(0.0f, log2f(f0 / 98.0f) * 1.0f));
    int r0 = reg >= 3.0f ? 2 : (int)reg;
    float rf = reg - (float)r0;
    float vw = fminf(2.0f, fmaxf(0.0f, vowel * 2.0f));
    int v0 = vw >= 2.0f ? 1 : (int)vw;
    float vf = vw - (float)v0;
    for (int k = 0; k < 5; k++) {
        for (int w = 0; w < 2; w++) {
            float a = FORMANT[r0][v0][w][k] + (FORMANT[r0][v0 + 1][w][k] - FORMANT[r0][v0][w][k]) * vf;
            float b = FORMANT[r0 + 1][v0][w][k] + (FORMANT[r0 + 1][v0 + 1][w][k] - FORMANT[r0 + 1][v0][w][k]) * vf;
            (w ? BW : F)[k] = a + (b - a) * rf;
        }
    }
    /* Above the first formant, a singer opens up to the note. */
    F[0] = fmaxf(F[0], f0 * 1.05f);
    F[1] = fmaxf(F[1], F[0] * 1.4f);
}

/* Klatt's resonator, unity gain at DC: y = A x + B y1 + C y2. */
static void resonator(float F, float BW, float *c) {
    const float T = 1.0f / QUILT_SR;
    F = fminf(F, 0.45f * QUILT_SR);
    c[2] = -expf(-TWO_PI * BW * T);
    c[1] = 2.0f * expf(-PI * BW * T) * cosf(TWO_PI * F * T);
    c[0] = 1.0f - c[1] - c[2];
}

static void tract(choir_voice_t *c, const quilt_t *q, int inst, int snap) {
    float F[5], BW[5];
    formants(c->f0, q->charv[inst], F, BW);
    for (int k = 0; k < 5; k++) {
        if (snap) c->ff[k] = F[k], c->fb[k] = BW[k];
        else c->ff[k] += (F[k] - c->ff[k]) * 0.3f, c->fb[k] += (BW[k] - c->fb[k]) * 0.3f;
        /* Two vocal tracts, one a little longer: each ear a different singer. */
        resonator(c->ff[k] * c->shift[0], c->fb[k], c->cf[0][k]);
        resonator(c->ff[k] * c->shift[1], c->fb[k], c->cf[1][k]);
    }
}

/* The pulse's slope, from how gently it is sung: a gentle voice's upper
 * harmonics fall fast. A strong breath may take it past SOFT's own end. */
static void pulse(choir_voice_t *c, float soft_eff) {
    c->soft_eff = soft_eff;
    float a = fminf(0.94f, fmaxf(0.45f, 0.86f - 0.36f * soft_eff));
    c->a = a;
    c->aN1 = powf(a, (float)(c->N + 1));
    c->aN2 = c->aN1 * a;
    /* The fundamental held level, so SOFT is heard as tone more than level. */
    c->norm = 1.0f / a;
}

void choir_note_on(quilt_t *q, voice_t *v) {
    const int inst = v->inst;
    choir_voice_t *c = &v->choir;
    memset(c, 0, sizeof(*c));
    c->rng = 0x2C3A51E7u ^ (uint32_t)(v->note * 7919 + (int)(v->vel * 1000.0f));
    const float f0 = choir_freq(inst, v->note);
    c->f0 = f0;
    c->n = (int)slot(q, inst, "v_crowd", 3.0f);
    const float split = slot(q, inst, "v_split", 0.3f);
    /* Each singer a little apart: up to 14 cents either side. */
    static const float spread[3] = { 0.0f, -0.85f, 1.0f };
    for (int k = 0; k < 3; k++) {
        c->inc[k] = f0 * powf(2.0f, spread[k] * 14.0f * split / 1200.0f) / QUILT_SR;
        c->ph[k] = 0.5f + 0.5f * noise(&c->rng);
        c->vib_ph[k] = 0.5f + 0.5f * noise(&c->rng);
        c->vib_mul[k] = 1.0f + 0.07f * spread[k];
        c->drift_amt[k] = (k ? 4.0f : 1.5f) + 10.0f * split;
    }
    /* Harmonics up to the top of the band, with room for the vibrato and the
     * detune; never more than 64. */
    c->N = (int)fminf(64.0f, fmaxf(1.0f, 0.45f * QUILT_SR / (f0 * 1.1f)));
    c->shift[0] = c->n > 1 ? 0.975f : 1.0f;
    c->shift[1] = c->n > 1 ? 1.025f : 1.0f;
    /* The singers' mix into the two ears, and its level, so a crowd is
     * wider, not louder. */
    c->mix = c->n == 1 ? 1.0f : c->n == 2 ? 0.5f : 0.7f;
    c->crowd_norm = 1.0f / sqrtf(c->n == 1 ? 1.0f : 1.0f + c->mix * c->mix);

    /* The breath: up to the key's velocity in SWELL's time, 40 ms to 1.6 s. */
    const float swell = slot(q, inst, "v_swell", 0.4f);
    c->lvl = 0.45f + 0.55f * v->vel;
    c->att_k = 1.0f - expf(-1.0f / (0.04f * powf(40.0f, swell) * QUILT_SR));
    /* Released, the voices fall away in DECAY's time, 50 ms to 2 s. */
    c->rel_k = 1.0f - expf(-1.0f / (0.05f * powf(40.0f, q->g[G_DECAY]) * QUILT_SR));
    c->level = 0.007655f;
    pulse(c, fminf(1.0f, fmaxf(-0.3f, q->g[G_SOFT] + 1.2f * (0.75f - c->lvl))));
    tract(c, q, inst, 1);
    float pan = 0.5f + 0.25f * (float)(v->note - 60) / 24.0f;
    pan = fminf(fmaxf(pan, 0.0f), 1.0f);
    c->pan_l = cosf(pan * PI * 0.5f) * 1.41421356f;
    c->pan_r = sinf(pan * PI * 0.5f) * 1.41421356f;
}

void choir_render(quilt_t *q, voice_t *v, float *left, float *right, int frames) {
    choir_voice_t *c = &v->choir;
    const int inst = v->inst;
    const float dt = 1.0f / QUILT_SR;
    const int press_src = (int)slot(q, inst, "v_press", 0.0f);
    const float air = slot(q, inst, "v_air", 0.3f);
    const float soft = q->gs[G_SOFT];
    const float pk = 1.0f - expf(-1.0f / (PRESS_SMOOTH_S * QUILT_SR));

    /* SWAY: each singer's vibrato at SPEED, a little apart, coming in once
     * the note has spoken; and each singer's slow drift. Both move once a
     * block. */
    const float vdepth = fmaxf(q->gs[G_SWAY], q->modwheel) * 40.0f;
    const float vib_hz = 0.5f + 7.5f * q->gs[G_SPEED];
    const float t1 = c->t + (float)frames * dt;
    const float vin = fminf(1.0f, fmaxf(0.0f, (t1 - 0.3f) * 2.5f));
    const float drift_k = 1.0f - expf(-(float)frames / (0.4f * QUILT_SR));
    float inc[3];
    for (int k = 0; k < c->n; k++) {
        c->vib_ph[k] += vib_hz * c->vib_mul[k] * (float)frames * dt;
        c->vib_ph[k] -= (float)(int)c->vib_ph[k];
        if (c->drift_t[k] <= 0.0f) {
            c->drift_to[k] = noise(&c->rng) * c->drift_amt[k];
            c->drift_t[k] = 0.3f + 0.4f * (0.5f + 0.5f * noise(&c->rng));
        }
        c->drift_t[k] -= (float)frames * dt;
        c->drift[k] += (c->drift_to[k] - c->drift[k]) * drift_k;
        const float cents = vdepth * vin * fast_sin(c->vib_ph[k]) + c->drift[k];
        inc[k] = c->inc[k] * (1.0f + cents * (0.6931472f / 1200.0f));
    }
    /* Sung harder, brighter: the breath feeds the softness, and the pulse is
     * reshaped when it has moved. */
    {
        float p0 = c->env;
        if (v->got_press && press_src != 1) p0 = press_level(c->env, c->press_s, v->hand, press_src);
        if (v->held && p0 > 0.05f) {
            const float se = fminf(1.0f, fmaxf(-0.3f, soft + 1.2f * (0.75f - p0)));
            if (fabsf(se - c->soft_eff) > 0.02f) pulse(c, se);
        }
    }
    tract(c, q, inst, 0);

    /* The singers side by side, one to a lane. Each pulse needs the
     * fundamental's phasor and the Nth harmonic's; both turn by a fixed step
     * through the block, set afresh from the phase at its start so the two
     * never drift apart. */
    const float a = c->a, aN1 = c->aN1, aN2 = c->aN2, a2 = 1.0f + a * a, norm = c->norm;
    const float fN = (float)c->N;
    v4 zr = { 1, 1, 1, 1 }, zi = { 0 }, nr = { 1, 1, 1, 1 }, ni = { 0 };
    v4 rc = { 1, 1, 1, 1 }, rs = { 0 }, rcN = { 1, 1, 1, 1 }, rsN = { 0 };
    for (int k = 0; k < c->n; k++) {
        const float ph = c->ph[k], th = TWO_PI * inc[k];
        zi[k] = fast_sin(ph), zr[k] = fast_sin(ph + 0.25f);
        ni[k] = fast_sin(fN * ph), nr[k] = fast_sin(fN * ph + 0.25f);
        rc[k] = cosf(th), rs[k] = sinf(th);
        rcN[k] = cosf(fN * th), rsN[k] = sinf(fN * th);
        c->ph[k] += inc[k] * (float)frames;
        c->ph[k] -= (float)(int)c->ph[k];
    }
    const float mix = c->mix, out_g = c->level * c->crowd_norm;
    /* Breath noise: AIR, and more of it the softer the voice. */
    const float asp = (air * 0.9f + 0.15f * fmaxf(0.0f, c->soft_eff)) * 1.2f;
    const int n_s = c->n;
    /* What changes sample by sample, kept out of memory: the output's
     * writes could otherwise be any of it. */
    const int held = v->held, use_press = v->got_press && press_src != 1;
    const float press = v->press, fade_step = v->fade_step, pan_l = c->pan_l, pan_r = c->pan_r;
    const float env_to = held ? c->lvl : 0.0f, env_k = held ? c->att_k : c->rel_k;
    float env = c->env, press_s = c->press_s, nz_lp = c->nz_lp, fade = v->fade, hand = v->hand;
    uint32_t rng = c->rng;
    float z[2][CHOIR_FORMANTS][2], cfs[2][CHOIR_FORMANTS][3];
    memcpy(z, c->z, sizeof(z));
    memcpy(cfs, c->cf, sizeof(cfs));
    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        env += (env_to - env) * env_k;
        float p = env;
        if (use_press) {
            press_s += ((held ? press : 0.0f) - press_s) * pk;
            hand = fminf(1.0f, hand + PRESS_HAND_STEP);
            p = press_level(env, press_s, hand, press_src);
        }
        const float nz = noise(&rng);
        nz_lp += (nz - nz_lp) * 0.5f;
        /* sum_{h=1..N} a^h sin(h th), Moorer's closed form */
        const v4 sN1 = ni * zr + nr * zi;
        v4 s = (a * zi - aN1 * sN1 + aN2 * ni) / (a2 - 2.0f * a * zr);
        /* the breath, while the folds are open */
        s = s * norm + (asp * nz_lp) * (0.6f + 0.4f * zr);
        v4 t = zr * rc - zi * rs;
        zi = zr * rs + zi * rc;
        zr = t;
        t = nr * rcN - ni * rsN;
        ni = nr * rsN + ni * rcN;
        nr = t;
        float in[2];
        if (n_s == 1) in[0] = in[1] = s[0];
        else if (n_s == 2) in[0] = s[0] + mix * s[1], in[1] = s[1] + mix * s[0];
        else in[0] = mix * s[0] + s[1], in[1] = mix * s[0] + s[2];
        float y[2];
        for (int e = 0; e < 2; e++) {
            float x = in[e];
            for (int k = 0; k < 5; k++) {
                const float *cf = cfs[e][k];
                float r = cf[0] * x + cf[1] * z[e][k][0] + cf[2] * z[e][k][1];
                z[e][k][1] = z[e][k][0];
                z[e][k][0] = r;
                x = r;
            }
            y[e] = x * p * out_g * fade;
        }
        if (fade_step > 0.0f) fade = fmaxf(0.0f, fade - fade_step);
        const float m = fmaxf(fabsf(y[0]), fabsf(y[1]));
        if (m > peak) peak = m;
        left[n] += y[0] * pan_l;
        right[n] += y[1] * pan_r;
    }
    c->env = env, c->press_s = press_s, c->nz_lp = nz_lp, v->fade = fade, v->hand = hand;
    c->rng = rng;
    memcpy(c->z, z, sizeof(z));
    c->t = t1;
    v->peak = peak;
    if ((!v->held && c->env < 1e-4f && peak < 1e-5f) || (v->fade_step > 0.0f && v->fade <= 0.0f))
        v->active = 0;
}
