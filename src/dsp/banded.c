/*
 * The banded waveguide engine: Bowed Vibes, Glass Harmonica, Singing Bowl.
 * DESIGN.md, Banded waveguide.
 *
 * Essl & Cook's banded waveguides: an inharmonic object as one loop per
 * strong mode, each a delay of one period of that mode and a narrow
 * band-pass at it, so the loop rings at the mode and nowhere else. The
 * band-pass has no phase at its centre, so each band is exactly in tune.
 * Where the bow (or the wet finger) touches, the velocity is the bands' sum;
 * the bow and the object stick or slip there (the bowed strings' friction,
 * waveguide.c), and what the bow gives drives every band. A plain bank of
 * modes cannot be bowed: it has no loop to sustain.
 *
 * Pressure is the pad, or an automatic swell (PRESS). HIT mixes in a mallet
 * strike at the start, so the bowl or the bar can be struck and then rubbed.
 * The mode ratios are Essl & Cook's: a tuned bar, a wine glass, and a
 * Tibetan bowl, whose modes come in near-degenerate pairs that beat.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define BAND_MASK (BANDED_LINE - 1)

typedef enum { DC_GRIP, DC_WET, DC_BEAT } banded_char_t;

typedef struct {
    const char *name;
    float gain;
    int lowest, highest;
    int nmodes;
    float ratio[BANDED_MODES], amp[BANDED_MODES];
    int paired;                  /* modes come in near-degenerate pairs (the bowl) */
    float t60_ref, f_ref;        /* the fundamental's ring once the bow lifts, at DECAY's middle */
    banded_char_t ch;
} banded_recipe_t;

static const banded_recipe_t RECIPES[] = {
    /* A vibraphone bar, undercut to 1 : 4 : 10.7 : 18 (Essl & Cook's tuned
     * bar), bowed on its end. */
    { .name = "Bowed Vibes", .gain = 0.794f, .lowest = 53, .highest = 89, .nmodes = 4,
      .ratio = { 1.0f, 4.0198f, 10.7184f, 18.0697f }, .amp = { 1.0f, 0.5f, 0.25f, 0.12f },
      .t60_ref = 6.0f, .f_ref = 349.0f, .ch = DC_GRIP },
    /* A wine glass, rubbed with a wet finger (Franklin's instrument). */
    { .name = "Glass Harmonica", .gain = 0.575f, .lowest = 55, .highest = 96, .nmodes = 5,
      .ratio = { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f }, .amp = { 1.0f, 0.4f, 0.2f, 0.1f, 0.05f },
      .t60_ref = 4.0f, .f_ref = 523.3f, .ch = DC_WET },
    /* A Tibetan bowl: each mode a pair a few cents apart, so it beats. The
     * fourth pair (8.48) is left out: rubbed, the bowl locks every pair but
     * the first, and that one is 0.12 of the second to start with. */
    { .name = "Singing Bowl", .gain = 0.646f, .lowest = 45, .highest = 81, .nmodes = 6,
      .ratio = { 1.0f, 1.0f, 2.79f, 2.79f, 5.13f, 5.13f },
      .amp = { 1.0f, 1.0f, 0.5f, 0.5f, 0.25f, 0.25f },
      .paired = 1, .t60_ref = 10.0f, .f_ref = 261.6f, .ch = DC_BEAT },
};

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))

static const banded_recipe_t *recipe_for(int inst) {
    for (int i = 0; i < N(RECIPES); i++)
        if (!strcmp(RECIPES[i].name, QUILT_INST[inst].name)) return &RECIPES[i];
    return NULL;
}

int banded_supports(int inst) { return recipe_for(inst) != NULL; }

float banded_freq(int inst, int note) {
    const banded_recipe_t *r = recipe_for(inst);
    if (r) {
        while (note < r->lowest) note += 12;
        while (note > r->highest) note -= 12;
    }
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

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

void banded_note_on(quilt_t *q, voice_t *v) {
    const int inst = v->inst;
    const banded_recipe_t *r = recipe_for(inst);
    banded_voice_t *b = &v->band;
    memset(b, 0, sizeof(*b));
    b->rng = 0x7F4A7C15u ^ (uint32_t)(v->note * 7919 + (int)(v->vel * 1000.0f));
    const float f0 = banded_freq(inst, v->note);
    const float charv = q->charv[inst];
    const float blur = slot(q, inst, "d_blur", 0.2f);
    b->f0 = f0;

    /* The bands: one per mode below 0.45 of the sample rate. BLUR widens
     * them, so the tone is less pure; BEAT parts the bowl's pairs. */
    const float dm = powf(2.0f, (q->g[G_DECAY] - 0.5f) * 7.0f);   /* DECAY: an eleventh to eleven times */
    int n = 0;
    for (int k = 0; k < r->nmodes; k++) {
        float ratio = r->ratio[k];
        const float split = 1.0f + 14.0f * (r->ch == DC_BEAT ? charv : 0.3f);   /* cents */
        if (r->paired && (k & 1)) ratio *= powf(2.0f, split / 1200.0f);
        if (r->paired && k == 1) b->beat_hz = f0 * (powf(2.0f, split / 1200.0f) - 1.0f);
        const float fk = f0 * ratio;
        if (fk > 0.45f * QUILT_SR || QUILT_SR / fk < 3.0f) break;
        band_t *d = &b->band[n];
        const float w = TWO_PI * fk / QUILT_SR;
        /* A tenth of the mode wide, or a fifth with BLUR: wide enough to
         * answer in a few milliseconds, narrow enough that the band's loop
         * cannot ring at its own overtones (2 fk is 20 dB down a pass). */
        const float bw = fk * (0.08f + 0.12f * blur);
        const float rr = expf(-PI * bw / QUILT_SR);
        d->b0 = (1.0f - rr * rr) * 0.5f;
        d->a1 = -2.0f * rr * cosf(w);
        d->a2 = rr * rr;
        /* Read before the write: the period of the mode, whole samples and
         * a first-order allpass for the rest (Thiran; its part kept between
         * a half and one and a half samples, where it is most accurate),
         * fixed for the note. */
        const float D = QUILT_SR / fk;
        d->li = (int)(D - 0.5f);
        const float frac = D - (float)d->li;
        d->ap_c = (1.0f - frac) / (1.0f + frac);
        d->amp = r->amp[k];
        /* The bow's first grip sets the object moving: a little of the mode
         * already in the band, so the friction takes hold at once instead of
         * growing from nothing. */
        const float seed = 0.03f * (0.3f + 0.7f * v->vel) * d->amp;
        for (int j = 1; j <= d->li + 1 && j < BANDED_LINE; j++)
            d->line[(-j) & BAND_MASK] = seed * sinf(-w * (float)j);
        d->ap_x1 = d->line[(-d->li - 1) & BAND_MASK];
        d->ap_y1 = seed * sinf(-w * (D + 1.0f));
        /* Bowed, the band keeps nearly all of a period; lifted, it rings for
         * DECAY, the upper modes shorter. */
        d->g_play = 0.9995f;
        const float t60 = r->t60_ref * powf(f0 / r->f_ref, -0.5f) * dm / sqrtf(ratio);
        /* The band-pass holds energy too, so the loop loses only part of
         * what the gain takes each period: (1-r) L / (1 + (1-r)(L-1)) of
         * it, from where the loop's pole falls. Ask for that much more. */
        const float lose = 1.0f - powf(10.0f, -3.0f / (t60 * fk));
        const float share = (1.0f - rr) * D / (1.0f + (1.0f - rr) * (D - 1.0f));
        d->g_rel = fmaxf(0.0f, 1.0f - lose / share);
        d->g = d->g_play;
        n++;
    }
    b->n = n;

    const float swell = slot(q, inst, "d_swell", 0.4f);
    b->lvl = 0.4f + 0.6f * v->vel;
    b->touch = 0.4f + 0.6f * v->vel;   /* a gentle hand is a rounder one, too */
    b->att_k = 1.0f - expf(-1.0f / (0.03f * powf(40.0f, swell) * QUILT_SR));
    b->rel_k = 1.0f - expf(-1.0f / (0.03f * QUILT_SR));
    /* HIT: a mallet's strike at the start, scaled by the key's velocity. */
    b->hit = slot(q, inst, "d_hit", 0.0f) * (0.2f + 0.8f * v->vel);
    b->level = r->gain;
    float pan = 0.5f + 0.3f * (float)(v->note - 66) / 30.0f;
    pan = fminf(fmaxf(pan, 0.0f), 1.0f);
    b->pan_l = cosf(pan * PI * 0.5f);
    b->pan_r = sinf(pan * PI * 0.5f);
}

void banded_render(quilt_t *q, voice_t *v, float *left, float *right, int frames) {
    const banded_recipe_t *r = recipe_for(v->inst);
    banded_voice_t *b = &v->band;
    const int inst = v->inst;
    const float charv = q->charv[inst];
    const int press_src = (int)slot(q, inst, "d_press", 0.0f);
    const float bow = slot(q, inst, "d_bow", 0.4f);
    const float noise_a = slot(q, inst, "d_noise", 0.2f) * (r->ch == DC_WET ? 1.0f - 0.6f * charv : 1.0f);
    const float soft = q->gs[G_SOFT];
    const float pk = 1.0f - expf(-1.0f / (0.012f * QUILT_SR));
    /* The friction: GRIP presses the bow harder; a wet finger (WET) grips
     * smoothly and sings pure, a dry one squeaks; SOFT is a gentler hand. The grip is set against the
     * bow's speed, so the contact always sits on the falling part of the
     * friction curve, where it sustains the object, and the speed sets the
     * level, as for the jets. */
    float grip = 0.5f;
    if (r->ch == DC_GRIP) grip = 0.25f + 0.75f * charv;
    if (r->ch == DC_WET) grip = 0.2f + 0.6f * charv;
    /* The bowl's stick, soft, is a leather-wrapped one: it slips smoothly. */
    const float reach = (0.35f + 0.4f * grip) * (1.0f - (r->paired ? 0.45f : 0.25f) * soft);
    /* The bow's speed: BOW, and the pressure. */
    /* A firmer grip is a heavier bow, too: it drives the bar harder. */
    const float vmax = (0.08f + 0.25f * bow) * (r->ch == DC_GRIP ? 0.4f + 0.6f * charv : 1.0f);
    /* SOFT rounds what the contact gives, as the bow's corners are. */
    /* A wet finger slides smoothly, its corners softer still. */
    const float fc = fminf(b->f0 * 24.0f * powf(0.02f, soft) * (r->ch == DC_WET ? 1.0f - 0.5f * charv : 1.0f) * b->touch,
                           0.45f * QUILT_SR);
    const float rk = 1.0f - expf(-TWO_PI * fc / QUILT_SR);
    /* What the rounding takes from f0, back; and most of what a light grip
     * takes from the level, so GRIP bites more than it swells. */
    const float comp = (1.0f + (b->f0 / fc) * (b->f0 / fc)) * powf(0.55f / reach, 0.5f);
    /* The bowl rubbed: the stick travels round the rim, feeding one member of
     * each pair and then the other, at the pairs' beat (BEAT), and each
     * pattern faces the listener in turn, so the bowl wah-wahs even while
     * the stick pulls the pair onto one note. */
    float wah0 = 0.0f, wah1 = 0.0f;
    if (r->paired) {
        b->wah_ph += b->beat_hz * (float)frames / QUILT_SR;
        b->wah_ph -= (float)(int)b->wah_ph;
        wah0 = fast_sin(b->wah_ph) * (r->ch == DC_BEAT ? charv : 0.3f);
        wah1 = fast_sin(b->wah_ph + b->beat_hz * (float)frames / QUILT_SR) * (r->ch == DC_BEAT ? charv : 0.3f);
    }

    /* Held, the bands keep nearly all; lifted, they ring for DECAY: each
     * band's gain glides there over about 25 ms, a block at a time. */
    const float glide = 1.0f - powf(0.999f, (float)frames);
    for (int k = 0; k < b->n; k++) {
        band_t *d = &b->band[k];
        d->g += ((v->held ? d->g_play : d->g_rel) - d->g) * glide;
    }

    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        b->env += ((v->held ? b->lvl : 0.0f) - b->env) * (v->held ? b->att_k : b->rel_k);
        float p = b->env;
        if (v->got_press && press_src != 1) {
            b->press_s += ((v->held ? v->press : 0.0f) - b->press_s) * pk;
            p = press_src == 0 ? fmaxf(b->press_s, 0.25f * b->env) : 0.5f * (b->env + b->press_s);
        }
        /* Released, the hand leaves in about 15 ms and the object rings. */
        b->lift += ((v->held ? 1.0f : 0.0f) - b->lift) * (v->held ? 1.0f : 0.0015f);
        const float contact = fminf(1.0f, p * 6.0f) * b->lift;

        /* The velocity where the bow touches: the bands' sum. */
        float vel = 0.0f;
        for (int k = 0; k < b->n; k++) {
            band_t *d = &b->band[k];
            const float x = d->line[(d->w - d->li) & BAND_MASK];
            d->back = d->ap_c * (x - d->ap_y1) + d->ap_x1;
            d->ap_x1 = x;
            d->ap_y1 = d->back;
            vel += d->back * d->amp;
        }
        /* NOISE: the friction's grain, a roughness in the bow's speed. */
        const float nz = noise(&b->rng);
        b->nz_lp += (nz - b->nz_lp) * 0.2f;
        const float vb = vmax * p * (1.0f + noise_a * 0.05f * b->nz_lp);
        const float dv = vb - vel;
        const float x = fabsf(dv * reach / fmaxf(vb, 0.002f)) + 0.75f, t = 1.0f / (x * x);
        float u = dv * fminf(1.0f, t * t) * contact;
        /* HIT: the mallet, a short pulse into every band. */
        if (b->hit > 1e-4f) {
            u += b->hit * 0.5f;
            b->hit *= 0.5f;
        }

        /* The rub feeds each pair's members in turn (no wah off the bowl). */
        const float wah = wah0 + (wah1 - wah0) * (float)n / (float)frames;
        const float uf[2] = { u * (1.0f + wah), u * (1.0f - wah) };
        float yf[2] = { 0.0f, 0.0f };
        for (int k = 0; k < b->n; k++) {
            band_t *d = &b->band[k];
            const float in = uf[k & 1] * d->amp + d->g * d->back;
            const float o = d->b0 * (in - d->x2) - d->a1 * d->y1 - d->a2 * d->y2;
            d->x2 = d->x1;
            d->x1 = in;
            d->y2 = d->y1;
            d->y1 = o;
            d->line[d->w] = o;
            d->w = (d->w + 1) & BAND_MASK;
            /* The pair's two patterns face the listener in turn as the rub
             * travels, so what is heard of each follows it too. */
            yf[k & 1] += o * d->amp;
        }
        const float y = yf[0] * (1.0f + wah) + yf[1] * (1.0f - wah);
        b->r1 += (y - b->r1) * rk;
        b->r2 += (b->r1 - b->r2) * rk;
        float out = (b->r2 * comp + (nz - b->nz_lp) * noise_a * p * contact * 0.04f) * b->level * v->fade;
        if (v->fade_step > 0.0f) v->fade = fmaxf(0.0f, v->fade - v->fade_step);
        const float m = fabsf(out);
        if (m > peak) peak = m;
        left[n] += out * b->pan_l;
        right[n] += out * b->pan_r;
    }
    v->mv.peak = peak;
    if ((!v->held && b->env < 1e-4f && peak < 1e-5f) || (v->fade_step > 0.0f && v->fade <= 0.0f))
        v->active = 0;
}
