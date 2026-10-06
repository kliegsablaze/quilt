/*
 * The air engine: Flute, Pan Flute, Ocarina, Recorder, Clarinet, Harmonium.
 * DESIGN.md, Air.
 *
 *   Jet       (flute, recorder, pan flute) Cook's STK flute: the breath
 *             less part of what the bore sends back, delayed along the jet,
 *             through the jet's cubic x(x^2 - 1), and the bore's own end
 *             reflection, into the bore. The bore is cut to two thirds of
 *             the note, so the note is its second resonance, as STK does:
 *             that keeps the jet from falling to the lower one. A stopped
 *             pipe (pan flute) reflects the other way, so odd harmonics.
 *             The ocarina's vessel has no harmonic overtones: the same jet
 *             and bore, with an end loss so heavy that only the fundamental
 *             rings, so nearly a sine.
 *   Reed      (clarinet) STK's clarinet: a cylindrical bore, half the period,
 *             and a reed whose opening follows the pressure across it
 *             (offset + slope dp, clipped), a valve on the breath.
 *   Free reed (harmonium) the reed's swing grows toward a size the bellows
 *             set, in the reed's own time; the sound is the air through the
 *             slot, which flows only while the reed is clear of it, through
 *             the reed chamber's resonance. Exactly in tune, and cheap.
 *
 * Breath is pad pressure, or an automatic swell (PRESS), as for the bow.
 * The jets and the clarinet's bore are tuned from a measured table: their
 * pitch is not just the bore's, because the jet's own delay pulls it.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define BORE_MASK (AIR_BORE - 1)
#define JET_MASK (AIR_JET - 1)
#ifndef ER
#define ER 0.92f
#endif


typedef enum { AK_JET, AK_STOPPED, AK_REED, AK_FREE } air_kind_t;
typedef enum { AC_COVER, AC_PUFF, AC_PURE, AC_REED, AC_BEAT } air_char_t;

typedef struct {
    const char *name;
    air_kind_t kind;
    air_char_t ch;
    float gain;
    int lowest, highest;          /* keys outside sound an octave in */
    float breath;                 /* full breath, in the model's pressure */
    float lp_pole;                /* the bore's end loss: its corner, in partials, at EDGE's middle */
    float noise;                  /* breath noise at AIR full */
    float vib_cents;              /* SWAY full */
} air_recipe_t;

static const air_recipe_t RECIPES[] = {
    { .name = "Flute", .kind = AK_JET, .ch = AC_COVER, .gain = 0.0249f, .lowest = 59, .highest = 96,
      .breath = 1.0f, .lp_pole = 6.0f, .noise = 1.0f, .vib_cents = 30 },
    { .name = "Pan Flute", .kind = AK_STOPPED, .ch = AC_PUFF, .gain = 0.0364f, .lowest = 55, .highest = 91,
      .breath = 1.0f, .lp_pole = 5.0f, .noise = 1.5f, .vib_cents = 25 },
    { .name = "Ocarina", .kind = AK_JET, .ch = AC_PURE, .gain = 0.0289f, .lowest = 60, .highest = 89,
      .breath = 1.0f, .lp_pole = 2.0f, .noise = 0.6f, .vib_cents = 20 },
    { .name = "Recorder", .kind = AK_JET, .ch = AC_PUFF, .gain = 0.0207f, .lowest = 60, .highest = 96,
      .breath = 1.0f, .lp_pole = 5.0f, .noise = 1.1f, .vib_cents = 15 },
    { .name = "Clarinet", .kind = AK_REED, .ch = AC_REED, .gain = 0.185f, .lowest = 50, .highest = 91,
      .breath = 1.0f, .lp_pole = 8.0f, .noise = 0.15f, .vib_cents = 30 },
    { .name = "Harmonium", .kind = AK_FREE, .ch = AC_BEAT, .gain = 0.127f, .lowest = 29, .highest = 96,
      .breath = 1.0f, .lp_pole = 6.0f, .noise = 0.1f, .vib_cents = 0 },
};

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))

static const air_recipe_t *recipe_for(int inst) {
    for (int i = 0; i < N(RECIPES); i++)
        if (!strcmp(RECIPES[i].name, QUILT_INST[inst].name)) return &RECIPES[i];
    return NULL;
}

int air_supports(int inst) { return recipe_for(inst) != NULL; }

/* Whether SWAY is the breath's vibrato (the winds) or the shared tremolo (the
 * harmonium's tremulant). */
int air_sway_is_vibrato(int inst) {
    const air_recipe_t *r = recipe_for(inst);
    return r && r->kind != AK_FREE;
}

float air_freq(int inst, int note) {
    const air_recipe_t *r = recipe_for(inst);
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

/* tanh, to within about 2 %: a rational fit, flat beyond 3. */
static inline float soft_tanh(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* A Lagrange read's whole-sample offset and four weights, worked out once
 * a block while the delay holds still. */
typedef struct { int i; float h[4]; } lag_t;
static inline lag_t lag_at(float D) {
    lag_t l;
    l.i = (int)D;
    float d = D - (float)l.i + 1.0f, dm1 = d - 1.0f, dm2 = d - 2.0f, dm3 = d - 3.0f;
    l.h[0] = -dm1 * dm2 * dm3 * (1.0f / 6.0f);
    l.h[1] = d * dm2 * dm3 * 0.5f;
    l.h[2] = -d * dm1 * dm3 * 0.5f;
    l.h[3] = d * dm1 * dm2 * (1.0f / 6.0f);
    return l;
}
static inline float lag_read(const float *line, int mask, int w, const lag_t *l) {
    return l->h[0] * line[(w - l->i + 1) & mask] + l->h[1] * line[(w - l->i) & mask] +
           l->h[2] * line[(w - l->i - 1) & mask] + l->h[3] * line[(w - l->i - 2) & mask];
}

/* Everything a voice's softness sets: the bore's end loss and length, the
 * jet's threshold, offset and level make-up, the reed's rounding. SOFT and
 * the breath both feed it, a gentle breath sounding as a soft player does,
 * so it is worked out again whenever the breath moves the softness. A strong
 * breath may take it a little below zero, past SOFT's own end. */
static void shape(const air_recipe_t *r, air_voice_t *a, float soft) {
    const float f0 = a->f0, charv = a->charv, edge = a->edge;
    const float P = QUILT_SR / f0, w0 = TWO_PI * f0 / QUILT_SR;
    a->soft_eff = soft;
    /* EDGE: the end loss, its corner a number of partials up, so every
     * register loses alike: about the 6th partial at the middle, from a
     * sixth of that to six times end to end; the flute's lip (COVER) brings
     * it down, and a gentle breath excites the upper resonances less. */
    float kc = r->lp_pole * powf(6.0f, (edge - 0.5f) * 2.0f);
    if (r->ch == AC_COVER) kc *= 1.0f - 0.88f * charv;
    if (r->kind == AK_JET || r->kind == AK_STOPPED) kc *= powf(r->kind == AK_STOPPED ? 0.2f : 0.35f, soft);
    /* Never below the second partial, the third for a reed, which needs
     * more of its bore: a corner too low starves it. */
    kc = fmaxf(kc, r->kind == AK_REED ? 3.0f : 1.5f);
    a->lpa = expf(-TWO_PI * fminf(f0 * kc, 0.45f * QUILT_SR) / QUILT_SR);
    const float lpd = atan2f(a->lpa * sinf(w0), 1.0f - a->lpa * cosf(w0)) / w0;
    a->comp = 1.0f;
    if (r->kind == AK_JET || r->kind == AK_STOPPED) {
        /* An open pipe's round trip is the period, and every harmonic is
         * a resonance; a stopped pipe's is half, inverted, so odd ones. */
        float bore = r->kind == AK_STOPPED ? P * 0.5f : P;
        /* COVER: the lip over the hole lengthens the pipe's open end, so
         * the note sinks, up to 10 cents. */
        if (r->ch == AC_COVER) bore *= powf(2.0f, 10.0f * charv / 1200.0f);
        /* Less the end loss's and the DC blocker's phase delay at f0, so the
         * bore is the same share of the period in every register. */
        const float dcd = -(atan2f(sinf(w0), 1.0f - cosf(w0)) - atan2f(0.995f * sinf(w0), 1.0f - 0.995f * cosf(w0))) / w0;
        a->D = fmaxf(2.0f, bore - lpd - dcd);
        /* The jet's gain at which the pipe just speaks: what the bore keeps
         * of a period at f0, against what the jet's low-pass passes. */
        const float hb = (1.0f - a->lpa) / sqrtf(1.0f - 2.0f * a->lpa * cosf(w0) + a->lpa * a->lpa);
        const float hj = a->jk / sqrtf(1.0f - 2.0f * (1.0f - a->jk) * cosf(w0) + (1.0f - a->jk) * (1.0f - a->jk));
        a->jet_th = (1.0f - ER * hb) / fmaxf(hj, 1e-3f);
        /* The jet strikes a little to one side of the edge, which is where
         * the even harmonics come from; a stopped pipe has few, the
         * ocarina's PURE centres it, and so does a gentle breath. */
        a->offset = r->ch == AC_PURE ? 0.3f * (1.0f - charv) : r->kind == AK_STOPPED ? 0.12f : 0.5f;
        a->offset *= 1.0f - 0.6f * soft;
        /* PURE also softens the vessel's jet, so it barely clips; the lip
         * (COVER) is partly in the flute's jet's way. */
        a->jet_mul = r->ch == AC_PURE ? 1.0f - 0.5f * charv : r->ch == AC_COVER ? 1.0f - 0.35f * charv : 1.0f;
        /* SOFT sets how far above its threshold the jet runs, 1.35 to 5.15
         * times; nearer, it sounds quieter, and most of that is given back,
         * so SOFT is heard as tone more than level. */
        a->over = fmaxf(1.35f, (1.35f + 3.8f * (1.0f - soft)) * a->jet_mul);
        a->comp = powf((1.35f + 3.8f * 0.4f) * a->jet_mul / a->over, 0.9f);
    } else if (r->kind == AK_REED) {
        /* Half the period round the bore, less the end loss's phase delay. */
        a->D = fmaxf(2.0f, P * 0.5f - lpd);
        /* SOFT, and a harder reed (REED), which closes less fully: rounder
         * pulses, a smoothing of what leaves the bore; what it takes from
         * f0 given back. */
        const float fc = fminf(f0 * 30.0f * powf(0.015f, soft) * powf(0.2f, charv), 0.45f * QUILT_SR);
        a->soft_k = 1.0f - expf(-TWO_PI * fc / QUILT_SR);
        a->comp = powf(1.0f + (f0 / fc) * (f0 / fc), 0.8f);
    } else {
        /* The free reed: a soft reed's motion is rounder. */
        const float fc = fminf(f0 * 40.0f * powf(0.018f, soft), 0.45f * QUILT_SR);
        a->soft_k = 1.0f - expf(-TWO_PI * fc / QUILT_SR);
    }
}

void air_note_on(quilt_t *q, voice_t *v) {
    const int inst = v->inst;
    const air_recipe_t *r = recipe_for(inst);
    air_voice_t *a = &v->air;
    memset(a, 0, sizeof(*a));
    a->rng = 0x51ED27A3u ^ (uint32_t)(v->note * 7919 + (int)(v->vel * 1000.0f));
    const float f0 = air_freq(inst, v->note);
    const float charv = q->charv[inst];
    const float edge = slot(q, inst, "a_edge", 0.4f);
    a->f0 = f0;
    /* The breath and the chiff, coloured by the mouthpiece and the edge. */
    tint_set(&a->nz_t, fminf(fmaxf(3.0f * f0, 600.0f), 2000.0f), 0.7f);
    tint_set(&a->chiff_t, fminf(fmaxf(5.0f * f0, 1000.0f), 3500.0f), 1.0f);

    /* The breath: up to the key's velocity in SWELL's time, 20 ms to 1.2 s. */
    const float swell = slot(q, inst, "a_swell", 0.2f);
    a->lvl = 0.45f + 0.55f * v->vel;
    a->att_k = 1.0f - expf(-1.0f / (0.02f * powf(60.0f, swell) * QUILT_SR));
    /* Released, the breath stops in DECAY's time, 20 ms to 1 s. */
    a->rel_k = 1.0f - expf(-1.0f / (0.02f * powf(50.0f, q->g[G_DECAY]) * QUILT_SR));
    /* ONSET: the tongue, a push of breath at the start. */
    a->onset = slot(q, inst, "a_onset", 0.3f) * (0.4f + 0.6f * v->vel);

    const float P = QUILT_SR / f0;
    if (r->kind == AK_JET || r->kind == AK_STOPPED) {
        const float w0 = TWO_PI * f0 / QUILT_SR;
        /* The jet in step with the bore at f0: its inversion (half a
         * period), less its low-pass's lag at 0.8 f0, so it pulls the pitch
         * nowhere. */
        const float jk = 1.0f - expf(-TWO_PI * f0 * 0.8f / QUILT_SR);
        const float lag = atan2f((1.0f - jk) * sinf(w0), 1.0f - (1.0f - jk) * cosf(w0)) / w0;   /* samples */
        float jr = 0.5f - lag / P;
        /* Read after the write, so a sample more. */
        a->Dj = fmaxf(2.0f, P * jr + 1.0f);
        a->jk = jk;
        a->js = -1.0f;
    } else {
        /* The free reed: its swing, and the second rank's (BEAT, céleste). */
        a->inc[0] = f0 / QUILT_SR;
        a->inc[1] = f0 * powf(2.0f, (2.0f + 10.0f * charv) / 1200.0f) / QUILT_SR;
        a->ph[1] = 0.37f;
        a->rank2 = charv > 0.01f ? 1.0f : 0.0f;
        /* The reed's own speaking time, shorter up the keys. */
        a->speak_k = 1.0f - expf(-1.0f / (0.03f * powf(f0 / 261.6f, -0.5f) * QUILT_SR));
        /* The reed chamber: a resonance near 1.5 kHz, higher with EDGE. */
        float fc = 700.0f * powf(6.0f, edge), rr = expf(-PI * fc / 2.0f / QUILT_SR);
        a->cav_c = 2.0f * rr * cosf(TWO_PI * fc / QUILT_SR);
        a->cav_r2 = rr * rr;
        a->cav_g = 1.0f - rr;
    }

    a->surge = 2.0f;
    /* PUFF: the chiff, a burst of edge noise as the pipe speaks. */
    a->chiff = r->ch == AC_PUFF ? charv * (0.4f + 0.6f * v->vel) : 0.0f;
    a->level = r->gain;
    /* The breath noise is heard against the note's own level. */
    a->noise_out = 1.0f;
    a->edge = edge;
    a->charv = charv;
    a->soft_eff = -1.0f;
    shape(r, a, fminf(1.0f, fmaxf(-0.3f, q->g[G_SOFT] + 1.4f * (0.75f - a->lvl))));
    a->Db_was = a->D;
    a->Dj_was = a->Dj;
    a->comp_was = a->comp;
    a->soft_k_was = a->soft_k;
    a->y0_was = 0.0f;
    a->jg_was = a->jet_th * a->over;
    a->vib_ph = 0.5f + 0.5f * noise(&a->rng);
    float pan = 0.5f + 0.25f * (float)(v->note - 66) / 30.0f;
    pan = fminf(fmaxf(pan, 0.0f), 1.0f);
    a->pan_l = cosf(pan * PI * 0.5f);
    a->pan_r = sinf(pan * PI * 0.5f);
}

void air_render(quilt_t *q, voice_t *v, float *left, float *right, int frames) {
    const air_recipe_t *r = recipe_for(v->inst);
    air_voice_t *a = &v->air;
    const int inst = v->inst;
    const float dt = 1.0f / QUILT_SR;
    const int press_src = (int)slot(q, inst, "a_press", 0.0f);
    const float air = slot(q, inst, "a_air", 0.26f) * r->noise;
    const float air_heard = noise_amount(slot(q, inst, "a_air", 0.26f), 6.0f) * r->noise;
    const float soft = q->gs[G_SOFT];
    /* SOFT: a gentler breath, less of it and more of it noise. */
    const float breath_max = r->breath * (1.15f - 0.35f * soft);
    const float pk = 1.0f - expf(-1.0f / (PRESS_SMOOTH_S * QUILT_SR));
    /* SWAY: breath vibrato at SPEED, in pitch and a little in level, coming
     * in after the note has spoken. */
    const float vdepth = fmaxf(q->gs[G_SWAY], q->modwheel) * r->vib_cents / 1200.0f * 0.6931f;
    const float vib_hz = 0.5f + 7.5f * q->gs[G_SPEED];
    const float t1 = a->t + (float)frames * dt;
    const float vin = fminf(1.0f, fmaxf(0.0f, (t1 - 0.3f) * 2.5f));
    a->vib_ph += vib_hz * (float)frames * dt;
    a->vib_ph -= (float)(int)a->vib_ph;
    const float vib = vdepth * vin * fast_sin(a->vib_ph);
    const float ratio = 1.0f + vib;
    const float chiff_k = expf(-1.0f / (0.025f * QUILT_SR));
    const float surge_k = expf(-1.0f / (0.05f * QUILT_SR));
    const float onset_k = expf(-1.0f / (0.03f * QUILT_SR));
    float onset = a->onset * expf(-a->t / 0.03f);
    /* Blown harder, brighter: the breath feeds the softness, and the voice
     * is reshaped when it has moved. */
    {
        float p0 = a->env;
        if (v->got_press && press_src != 1) p0 = press_level(a->env, a->press_s, v->hand, press_src);
        if (v->held && p0 > 0.05f) {
            const float se = fminf(1.0f, fmaxf(-0.3f, soft + 1.4f * (0.75f - p0)));
            if (fabsf(se - a->soft_eff) > 0.02f) shape(r, a, se);
        }
    }
    const float soft_e = a->soft_eff;

    /* What is worked out once a block (the vibrato, and the voice reshaped
     * as the breath moves) glides across it from where the last block
     * ended, so nothing steps: two reads of each line, faded between, and
     * the jet's gain, its offset, the reed's smoothing and the level make-up
     * ramped. */
    const float Db = fmaxf(2.0f, a->D / ratio), Dj = fmaxf(2.0f, a->Dj / ratio);
    const lag_t lb0 = lag_at(a->Db_was), lb = lag_at(Db), lj0 = lag_at(a->Dj_was), lj = lag_at(Dj);
    const int glide_b = Db != a->Db_was, glide_j = Dj != a->Dj_was;
    const float y0_to = a->offset * a->jamp * 1.57f, jg_to = a->jet_th * a->over;
    const float inv = 1.0f / (float)frames;
    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        const float x = (float)(n + 1) * inv;
        const float y0 = a->y0_was + (y0_to - a->y0_was) * x, ty0 = soft_tanh(y0);
        const float soft_k = a->soft_k_was + (a->soft_k - a->soft_k_was) * x;
        a->env += ((v->held ? a->lvl : 0.0f) - a->env) * (v->held ? a->att_k : a->rel_k);
        float p = a->env;
        if (v->got_press && press_src != 1) {
            a->press_s += ((v->held ? v->press : 0.0f) - a->press_s) * pk;
            v->hand = fminf(1.0f, v->hand + PRESS_HAND_STEP);
            p = press_level(a->env, a->press_s, v->hand, press_src);
        }
        onset *= onset_k;
        const float nz = noise(&a->rng);
        a->nz_lp += (nz - a->nz_lp) * 0.4f;
        /* A jet speaks only above a threshold, about 1.3 here, and runs out
         * past about 1.6; the breath moves within that, less of it softer. */
        /* The clarinet's reed speaks between about 0.45 and 0.7 of the
         * breath, a soft reed, or 0.62 and 0.85, a hard one; the breath
         * moves within the reed's own window, harder for a harder reed. */
        const float breath = (r->kind == AK_REED ? (0.49f + 0.17f * q->charv[inst] + 0.17f * p * (1.0f - 0.4f * soft)) *
                                                       fminf(1.0f, p * 4.0f)
                                                 : breath_max * p)
                             * (1.0f + onset * 0.6f) * (1.0f + 0.3f * vib * 20.0f);
        const float rand_p = breath * (1.0f + (air + 0.15f * soft_e) * a->nz_lp);
        float y;

        if (r->kind == AK_JET || r->kind == AK_STOPPED) {
            float o = lag_read(a->bore, BORE_MASK, a->bw, &lb);
            if (glide_b) o = lag_read(a->bore, BORE_MASK, a->bw, &lb0) * (1.0f - x) + o * x;
            a->lp += (1.0f - a->lpa) * (o - a->lp);
            float back = r->kind == AK_STOPPED ? -a->lp : a->lp;
            float d = back - a->dcx + 0.995f * a->dcy;
            a->dcx = back;
            a->dcy = d;
            back = d;
            /* The jet (Verge): the bore's sound deflects it, it carries the
             * deflection along to the edge, and the flow that goes in is
             * the breath times tanh of how far it is off the edge, the
             * edge a little to one side (y0), which gives the even
             * harmonics. Bounded, so no breath latches it. */
            a->jlp += (back - a->jlp) * a->jk;

            a->jet[a->jw] = a->jlp;
            a->jw = (a->jw + 1) & JET_MASK;
            float jv = lag_read(a->jet, JET_MASK, a->jw, &lj);
            if (glide_j) jv = lag_read(a->jet, JET_MASK, a->jw, &lj0) * (1.0f - x) + jv * x;
            /* The jet's deflection against the breath: a stronger breath
             * carries a stronger jet, so the shape holds and the level
             * follows the breath, a little brighter as it rises. */
            /* The attack's surge: a stronger jet for the first 50 ms or
             * so, which is what makes a pipe speak quickly. */
            float dev = (a->jg_was + (jg_to - a->jg_was) * x) * (1.0f + a->surge) *
                        a->js * jv / fmaxf(breath, 0.02f);
            a->surge *= surge_k;
            a->jamp += (fabsf(dev) - a->jamp) * 0.001f;
            /* About half the jet goes in on average, so the breath arriving
             * is a step of flow that sets the pipe ringing at once (the
             * bore's DC blocker takes the steady part out again); and breath
             * is never quite smooth: a floor of turbulence, whatever AIR is. */
            float j = breath * (soft_tanh(dev + y0) - ty0 + 0.02f * a->nz_lp + 0.5f) + rand_p * 0.05f;
            float in = j + ER * back;
            a->bore[a->bw] = in;
            a->bw = (a->bw + 1) & BORE_MASK;
            /* Heard as the bore sounds, at the mouth: what comes back. */
            y = back * 0.6f;
        } else if (r->kind == AK_REED) {
            /* The bore returns with the end's averaging loss, inverted. */
            float o = lag_read(a->bore, BORE_MASK, a->bw, &lb);
            if (glide_b) o = lag_read(a->bore, BORE_MASK, a->bw, &lb0) * (1.0f - x) + o * x;
            a->lp += (1.0f - a->lpa) * (o - a->lp);
            float pd = -0.95f * a->lp - rand_p;
            /* REED: a soft reed slams shut more readily, so it cuts the
             * breath off sharply and buzzes; a harder one, less: darker, as
             * players know. */
            const float give = 1.0f - q->charv[inst];   /* a soft reed gives */
            float refl = (0.7f - 0.1f * give) + (-0.3f - 0.25f * give) * pd;
            refl = fminf(1.0f, fmaxf(-1.0f, refl));
            float in = rand_p + pd * refl;
            a->bore[a->bw] = in;
            a->bw = (a->bw + 1) & BORE_MASK;
            /* The bell: what leaves is the bore's high-passed. */
            float d = in - a->dcx + 0.995f * a->dcy;
            a->dcx = in;
            a->dcy = d;
            /* What leaves follows the player's pressure; the reed alone
             * would hold its level whatever the breath. */
            /* SOFT: a gentle lip damps the reed, so it never quite slams
             * shut and the pressure's corners round, as a bow's do: a
             * smoothing of what the bore sends out. */
            a->flow_lp += (d - a->flow_lp) * soft_k;
            a->rlp2 += (a->flow_lp - a->rlp2) * soft_k;
            y = a->rlp2 * (0.05f + 0.95f * p);
        } else {
            /* The free reed. Its swing grows toward the bellows' size. */
            a->swing += (p * (1.0f + onset * 2.0f) - a->swing) * a->speak_k * (0.3f + p);
            float x0 = fast_sin(a->ph[0]) * a->swing;
            float x1 = fast_sin(a->ph[1]) * a->swing * a->rank2;
            for (int k = 0; k < 2; k++) {
                a->ph[k] += a->inc[k] * ratio;
                if (a->ph[k] >= 1.0f) a->ph[k] -= 1.0f;
            }
            /* Air flows only while the reed is clear of the slot, as the
             * square root of the pressure across it. */
            /* The harder the bellows, the more sharply the reed cuts the air
             * off, so the narrower the pulses and the brighter; SOFT the
             * opposite. */
            const float gap = fminf(0.85f, 0.05f + 0.75f * p * (1.0f - soft_e));   /* the reed must clear it */
            float flow = fmaxf(0.0f, x0 - gap * a->swing) + fmaxf(0.0f, x1 - gap * a->swing);
            flow *= sqrtf(fmaxf(0.0f, p)) * (1.0f + air * 2.0f * a->nz_lp);
            /* A soft reed's motion is rounder: SOFT smooths the pulses. */
            a->flow_lp += (flow - a->flow_lp) * soft_k;
            flow = a->flow_lp;
            /* What is heard is the flow, its steady part taken out, and the
             * reed chamber's resonance, which the flow's changes ring. */
            float dflow = flow - a->dcx;
            a->dcx = flow;
            a->dcy += (flow - a->dcy) * 0.002f;
            float c = a->cav_c * a->cav_y1 - a->cav_r2 * a->cav_y2 + a->cav_g * dflow;
            a->cav_y2 = a->cav_y1;
            a->cav_y1 = c;
            y = (c * 2.0f + (flow - a->dcy) * 0.3f) * 5.0f;
        }

        /* AIR: the breath itself, heard beside the note. */
        y += tint(&a->nz_t, nz) * air_heard * breath * a->noise_out;
        /* PUFF: the chiff, edge noise as the pipe speaks. */
        if (a->chiff > 1e-4f) {
            y += tint(&a->chiff_t, nz) * a->chiff * p * 4.0f;
            a->chiff *= chiff_k;
        }
        y *= a->level * (a->comp_was + (a->comp - a->comp_was) * x) * v->fade;
        if (v->fade_step > 0.0f) v->fade = fmaxf(0.0f, v->fade - v->fade_step);
        float m = fabsf(y);
        if (m > peak) peak = m;
        left[n] += y * a->pan_l;
        right[n] += y * a->pan_r;
        a->t += dt;
    }
    a->Db_was = Db;
    a->Dj_was = Dj;
    a->comp_was = a->comp;
    a->soft_k_was = a->soft_k;
    a->y0_was = y0_to;
    a->jg_was = jg_to;
    v->peak = peak;
    if ((!v->held && a->env < 1e-4f && peak < 1e-5f) || (v->fade_step > 0.0f && v->fade <= 0.0f))
        v->active = 0;
}
