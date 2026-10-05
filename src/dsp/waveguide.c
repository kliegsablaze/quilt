/*
 * The waveguide engine, plucked: Harp, Nylon Guitar, Pizzicato, Clavichord.
 * DESIGN.md, Waveguide.
 *
 * A string is a delay line closed on itself through its losses (Jaffe &
 * Smith's extended Karplus-Strong; Karjalainen, Valimaki & Tolonen):
 *
 *   loss        a one-pole low-pass with a gain: the fundamental falls 60 dB
 *               in its T60, and a partial at f in T60 / (1 + (f/fb)^2), fb
 *               being EDGE's brightness. Worked out per note, so the decay
 *               is the same however long the line is.
 *   stiffness   a first-order allpass (Van Duyne & Smith; Rauhala &
 *               Valimaki): more delay low than high, so the upper partials
 *               stretch sharp. STIFF sets where it turns over, relative to
 *               the note, so it stretches alike in every register.
 *   tuning      a third-order Lagrange read, the line's length less both
 *               filters' phase delay at f0, so the note is in tune at any
 *               setting. The read can move every sample: the clavichord's
 *               Bebung.
 *
 * The pluck is commuted (Smith; Valimaki, Laurson & Erkut): the string is
 * linear, so the body's response can come before it, folded into what
 * drives it. One impulse, the width of a fingertip or a nail (SOFT), tilted
 * as a plucked string's partials fall (1/k), runs through four of the body's
 * resonances (BODY), then a comb at the pluck point (SPOT) takes out the
 * partials that have a node there. The finger's own sound (NOISE) is added
 * beside the string.
 *
 * BLOOM, on the harp and the guitar, is a bank of open strings shared by
 * every note (wg_bank_t), driven by the bridge: the strings whose partials
 * meet the played note's take them up and ring on.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define LINE_MASK (WG_LINE - 1)
#define HIST_MASK (WG_HIST - 1)
#define BANK_MASK (WG_BANK_LINE - 1)

typedef enum { WC_BLOOM, WC_DEEP, WC_BEND } wg_char_t;

typedef struct {
    const char *name;
    float gain;
    int lowest;                   /* keys below sound an octave up, and so on */
    float t60_ref, f_ref, t60_slope;   /* fundamental T60 at f_ref, falling as (f/f_ref)^-slope */
    float fb_mult;                /* x EDGE's brightness: gut is darker than brass */
    float soft_mult;              /* x the hardest pluck's brightness: a nail, a pad, a tangent */
    float body_hz[4], body_t60[4], body_g[4];
    float deep_hz[4];             /* Pizzicato: the cello's body, DEEP's far end */
    float noise_hz, noise_ms, noise_gain;
    float cloth;                  /* the clavichord's listing cloth: extra loss when released, 1/s */
    int dampers;                  /* 1: a released note is always damped; 0: only by DAMP, the hand */
    float pan_spread;
    wg_char_t ch;
    int symp[WG_BANK_STRINGS];    /* BLOOM's open strings, as MIDI notes; 0 ends */
} wg_recipe_t;

static const wg_recipe_t RECIPES[] = {
    /* Gut and nylon, the fingertip, a long ring; the soundbox's few broad
     * modes. The bank is a chromatic harp's octave below middle C. */
    { .name = "Harp", .gain = 0.194f, .lowest = 24,
      .t60_ref = 7.0f, .f_ref = 261.6f, .t60_slope = 0.7f, .fb_mult = 1.0f, .soft_mult = 1.0f,
      .body_hz = { 140, 280, 520, 900 }, .body_t60 = { 0.12f, 0.10f, 0.07f, 0.05f },
      .body_g = { 0.6f, 0.5f, 0.35f, 0.2f },
      .noise_hz = 2500, .noise_ms = 5, .noise_gain = 1.0f,
      .dampers = 0, .pan_spread = 0.7f, .ch = WC_BLOOM,
      .symp = { 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59 } },
    /* Fingerstyle near the soundhole: the air mode at 100 Hz, the top's
     * first modes above it. The bank is the six open strings. */
    { .name = "Nylon Guitar", .gain = 0.56f, .lowest = 40,
      .t60_ref = 4.0f, .f_ref = 261.6f, .t60_slope = 0.6f, .fb_mult = 0.8f, .soft_mult = 1.0f,
      .body_hz = { 100, 205, 400, 580 }, .body_t60 = { 0.08f, 0.05f, 0.03f, 0.02f },
      .body_g = { 0.8f, 0.7f, 0.4f, 0.25f },
      .noise_hz = 3000, .noise_ms = 4, .noise_gain = 1.0f,
      .dampers = 0, .pan_spread = 0.3f, .ch = WC_BLOOM,
      .symp = { 40, 45, 50, 55, 59, 64 } },
    /* Gut strings plucked by the finger's pad and stopped by the other hand,
     * so short and dark; the viola's body, or the cello's (DEEP). */
    { .name = "Pizzicato", .gain = 1.07f, .lowest = 36,
      .t60_ref = 1.6f, .f_ref = 130.8f, .t60_slope = 0.5f, .fb_mult = 0.5f, .soft_mult = 0.7f,
      .body_hz = { 230, 400, 480, 1100 }, .body_t60 = { 0.10f, 0.08f, 0.08f, 0.05f },
      .body_g = { 0.8f, 0.6f, 0.6f, 0.3f },
      .deep_hz = { 100, 175, 210, 600 },
      .noise_hz = 1500, .noise_ms = 6, .noise_gain = 1.0f,
      .dampers = 1, .pan_spread = 0.3f, .ch = WC_DEEP },
    /* Brass struck by a tangent that stays on the string; a small board, and
     * the listing cloth that silences the rest of the string and the note
     * when the key comes up. */
    { .name = "Clavichord", .gain = 1.03f, .lowest = 29,
      .t60_ref = 3.5f, .f_ref = 261.6f, .t60_slope = 0.5f, .fb_mult = 1.5f, .soft_mult = 1.6f,
      .body_hz = { 250, 520, 1100, 2400 }, .body_t60 = { 0.06f, 0.05f, 0.04f, 0.03f },
      .body_g = { 0.5f, 0.5f, 0.4f, 0.3f },
      .noise_hz = 1800, .noise_ms = 8, .noise_gain = 1.0f,
      .cloth = 4.0f, .dampers = 1, .pan_spread = 0.3f, .ch = WC_BEND },
};

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))

static const wg_recipe_t *recipe_for(int inst) {
    for (int i = 0; i < N(RECIPES); i++)
        if (!strcmp(RECIPES[i].name, QUILT_INST[inst].name)) return &RECIPES[i];
    return NULL;
}

int waveguide_supports(int inst) { return recipe_for(inst) != NULL; }

static float slot(const quilt_t *q, int inst, const char *key, float fallback) {
    int i = quilt_shape_key_in(QUILT_INST[inst].shape, key);
    return i < 0 ? fallback : q->slot[inst][i];
}

/* The pitch a key sounds: equal temperament, folded into the instrument's
 * compass (a guitar has no string below E2) and below the top of the line. */
float waveguide_freq(int inst, int note) {
    const wg_recipe_t *r = recipe_for(inst);
    if (r) while (note < r->lowest) note += 12;
    while (note > 108) note -= 12;
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

/* Phase delays at w (radians per sample), in samples. */
static float lp_delay(float a, float w) { return atan2f(a * sinf(w), 1.0f - a * cosf(w)) / w; }
static float ap_delay(float c, float w) {
    float num = atan2f(-sinf(w), c + cosf(w)), den = atan2f(-c * sinf(w), 1.0f + c * cosf(w));
    return -(num - den) / w;
}

/* The one-pole loss's pole for a partial at f to ring T60 / (1 + (f/fb)^2):
 * for small w its extra loss per period is 4.34 a w^2 / (1-a)^2 dB. */
static float loss_pole(float fb, float t60, float f0) {
    float c = 60.0f * QUILT_SR * QUILT_SR / (4.343f * 4.0f * PI * PI * fb * fb * t60 * f0);
    return ((2.0f * c + 1.0f) - sqrtf(4.0f * c + 1.0f)) / (2.0f * c);
}

/* The largest pole the loss may have at f0 for a fundamental ringing t60. */
static double pole_cap(double budget, double cw) {
    const double l2 = pow(10.0, -0.9 * budget / 10.0), b = 1.0 - l2 * cw, k = 1.0 - l2;
    return (b - sqrt(b * b - k * k)) / k;
}

/* The loop's gain and pole for a fundamental that rings exactly t60: the
 * pole may take at most 90 % of the loss a period allows at f0, and the
 * gain the rest, so the loop stays below 1 at every frequency. */
static void loss(float fb, float t60, float f0, float *a, float *g) {
    const double w = 2.0 * 3.14159265358979 * f0 / QUILT_SR, cw = cos(w);
    const double budget = 60.0 / (t60 * f0);                 /* dB per period at f0 */
    const double l2 = pow(10.0, -0.9 * budget / 10.0);
    const double b = 1.0 - l2 * cw, k = 1.0 - l2;
    const double amax = (b - sqrt(b * b - k * k)) / k;        /* the pole that spends 90 % */
    double pa = loss_pole(fb, t60, f0);
    if (pa > amax) pa = amax;
    const double hlp = (1.0 - pa) / sqrt(1.0 - 2.0 * pa * cw + pa * pa);
    *a = (float)pa;
    *g = (float)(pow(10.0, -budget / 20.0) / hlp);
}

/* Third-order Lagrange weights for the fraction d in [1, 2). */
static inline void lagrange_h(float d, float *h) {
    float dm1 = d - 1.0f, dm2 = d - 2.0f, dm3 = d - 3.0f;
    h[0] = -dm1 * dm2 * dm3 * (1.0f / 6.0f);
    h[1] = d * dm2 * dm3 * 0.5f;
    h[2] = -d * dm1 * dm3 * 0.5f;
    h[3] = d * dm1 * dm2 * (1.0f / 6.0f);
}

/* Third-order Lagrange read, D samples back from the next write, D >= 2. */
static inline float lagrange(const float *line, int mask, int w, float D) {
    int i = (int)D;
    float d = D - (float)i + 1.0f;     /* within the four taps i-1 .. i+2, in [1, 2) */
    float dm1 = d - 1.0f, dm2 = d - 2.0f, dm3 = d - 3.0f;
    float h0 = -dm1 * dm2 * dm3 * (1.0f / 6.0f);
    float h1 = d * dm2 * dm3 * 0.5f;
    float h2 = -d * dm1 * dm3 * 0.5f;
    float h3 = d * dm1 * dm2 * (1.0f / 6.0f);
    return h0 * line[(w - i + 1) & mask] + h1 * line[(w - i) & mask] +
           h2 * line[(w - i - 1) & mask] + h3 * line[(w - i - 2) & mask];
}

static uint32_t xorshift(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static float noise(uint32_t *s) { return (float)(int32_t)xorshift(s) * (1.0f / 2147483648.0f); }

void waveguide_note_on(quilt_t *q, voice_t *v) {
    const int inst = v->inst;
    const wg_recipe_t *r = recipe_for(inst);
    wg_voice_t *s = &v->wg;
    memset(s, 0, sizeof(*s));
    s->rng = 0x2545F491u ^ (uint32_t)(v->note * 7919 + (int)(v->vel * 1000.0f));

    const float f0 = waveguide_freq(inst, v->note);
    const float w0 = TWO_PI * f0 / QUILT_SR;
    const float vel = v->vel;
    const float soft = q->g[G_SOFT];
    const float charv = q->charv[inst];
    const float spot = slot(q, inst, "p_spot", 0.3f);
    const float edge = slot(q, inst, "p_edge", 0.5f);
    const float stiff = slot(q, inst, "p_stiff", 0.3f);
    const float damp = slot(q, inst, "p_damp", 0.3f);
    const float deep = r->ch == WC_DEEP ? charv : 0.0f;
    s->f0 = f0;
    s->P = QUILT_SR / f0;
    s->body = slot(q, inst, "p_body", 0.5f);

    /* Losses: DECAY a factor of four either way; the cello rings a little
     * longer and darker than the viola. */
    const float dm = powf(2.0f, (q->g[G_DECAY] - 0.5f) * 4.0f);
    const float t60 = r->t60_ref * powf(f0 / r->f_ref, -r->t60_slope) * dm * (1.0f + 0.5f * deep);
    const float fb = 300.0f * powf(8000.0f / 300.0f, edge) * r->fb_mult * (1.0f - 0.5f * deep);
    loss(fb, t60, f0, &s->a, &s->g);
    const float sigma = 6.91f / t60;
    /* DAMP: from a hand that lets the string sigh away (about 2 s) to one
     * that stops it dead (about 50 ms); the clavichord's cloth on top. */
    s->gd = expf(-(sigma + 3.0f + 135.0f * damp * damp + r->cloth) / f0);
    s->dampable = r->dampers || damp > 0.01f;

    /* Stiffness: the allpass turns over at x radians, from the top of the
     * band (none) down to the note's fourth partial. */
    float x = powf(fminf(1.0f, 4.0f * w0), stiff);
    s->ac = stiff > 0.005f ? -(1.0f - x) : 0.0f;
    for (;;) {
        s->tau = lp_delay(s->a, w0) + (s->ac != 0.0f ? ap_delay(s->ac, w0) : 0.0f);
        s->D = s->P - s->tau;
        if (s->D >= 2.0f || s->ac == 0.0f) break;
        s->ac *= 0.5f;             /* the top notes' lines are too short for it all */
        if (s->ac > -0.01f) s->ac = 0.0f;
    }
    if (s->D < 2.0f) s->D = 2.0f;
    s->bend = 1.0f;

    /* The pluck: an impulse as wide as the fingertip (SOFT) and gentler for
     * a soft touch, then tilted to a plucked string's 1/k above f0. Its size
     * follows the period, so every register starts as strong. */
    const float nail = 7000.0f * r->soft_mult;
    float fc = nail * powf(250.0f / nail, soft) * (0.5f + vel) * (0.5f + vel);
    fc = fminf(fmaxf(fc, f0), 0.45f * QUILT_SR);
    s->lpk = 1.0f - expf(-TWO_PI * fc / QUILT_SR);
    s->ik = 1.0f - expf(-TWO_PI * 0.7f * f0 / QUILT_SR);
    /* Give back what the fingertip's width takes from f0, and a little of
     * what it takes above, so SOFT is heard as tone more than level. */
    const float lost = 1.0f + (f0 / fc) * (f0 / fc);
    const float hard = fminf(nail * (0.5f + vel) * (0.5f + vel), 0.45f * QUILT_SR);
    /* Fingers pull the strings this way and that, so a chord's plucks do
     * not all start in the same direction and stack into one spike. */
    q->pluck_dir ^= 1;
    s->amp = (q->pluck_dir ? 25.0f : -25.0f) * (0.1f + 0.9f * vel) * s->P / 168.6f * lost * powf(hard / fc, 0.12f);

    /* The pluck point: a comb at beta of the length, beta from by the bridge
     * to the middle, where the even partials vanish. Part of the level it
     * takes from the fundamental near the bridge is given back. */
    const float beta = 0.02f + 0.48f * powf(spot, 1.5f);
    s->comb_d = beta * s->P;
    s->comb_norm = powf(2.0f * sinf(PI * beta), -0.7f);

    /* The body: the direct path plus four band-passes, in phase with it at
     * their peaks, each 4 body_g (about 12 dB) over it with BODY full. */
    float longest = 0.0f;
    for (int i = 0; i < 4; i++) {
        float hz = r->body_hz[i];
        if (deep > 0.0f) hz *= powf(r->deep_hz[i] / r->body_hz[i], deep);
        float w = TWO_PI * hz / QUILT_SR, rr = expf(-6.91f / (r->body_t60[i] * QUILT_SR));
        s->res_c[i] = 2.0f * rr * cosf(w);
        s->res_r2[i] = rr * rr;
        s->res_g[i] = 4.0f * r->body_g[i] * s->body * (1.0f - rr);
        longest = fmaxf(longest, r->body_t60[i]);
    }
    s->ex_len = (int)(longest * QUILT_SR * 1.2f) + (int)s->comb_d + 64;
    if (s->ex_len > (int)(0.5f * QUILT_SR)) s->ex_len = (int)(0.5f * QUILT_SR);

    /* The finger on the string, the nail's click, the tangent's knock. */
    tint_set(&s->nz_t, r->noise_hz * 0.5f, 0.7f);
    s->nz_td = expf(-1.0f / (r->noise_ms * 0.001f * QUILT_SR));
    s->nz_env = 1.0f;
    s->nz_a = r->noise_gain * 2.5f * r->gain * (0.2f + 0.8f * vel) *
               noise_amount(slot(q, inst, "p_noise", 0.37f), 2.2f);

    s->level = r->gain;
    float pan = 0.5f + 0.5f * r->pan_spread * (float)(v->note - 64) / 40.0f;
    pan = fminf(fmaxf(pan, 0.0f), 1.0f);
    s->pan_l = cosf(pan * PI * 0.5f);
    s->pan_r = sinf(pan * PI * 0.5f);
}

void waveguide_render(quilt_t *q, voice_t *v, float *left, float *right, float *bridge, int frames) {
    const wg_recipe_t *r = recipe_for(v->inst);
    wg_voice_t *s = &v->wg;
    const int damped = s->dampable && !v->held && !q->pedal;
    /* The hand (or the damper) settles on the string over about 40 ms, the
     * loss moving evenly in rate, not in one step, which would drop the
     * string's level a round trip later, a click. */
    if (s->damp_x != (float)damped) {
        const float step = (float)frames / (0.04f * QUILT_SR);
        s->damp_x = damped ? fminf(1.0f, s->damp_x + step) : fmaxf(0.0f, s->damp_x - step);
    }
    const float g_to = s->damp_x <= 0.0f ? s->g : s->damp_x >= 1.0f ? s->gd
                     : expf((1.0f - s->damp_x) * logf(s->g) + s->damp_x * logf(s->gd));
    /* ... and within the block, sample by sample, from where the last ended. */
    if (s->g_was <= 0.0f) s->g_was = g_to;
    const float g_mul = g_to == s->g_was ? 1.0f : powf(g_to / s->g_was, 1.0f / (float)frames);
    float g = s->g_was;
    const float a = s->a, ac = s->ac;

    /* BEND: pad pressure on the key presses the tangent harder and the
     * string goes sharp, up to 60 cents (Bebung). A resting finger, the
     * lightest fifth of the pad, is in tune. */
    float bend_to = 1.0f;
    if (r->ch == WC_BEND && v->got_press && v->held) {
        const float push = fmaxf(0.0f, (v->press - 0.2f) / 0.8f);
        bend_to = powf(2.0f, q->charv[v->inst] * push * 60.0f / 1200.0f);
    }
    const float bk = 1.0f - expf(-1.0f / (0.008f * QUILT_SR));
    const int bending = fabsf(bend_to - s->bend) > 1e-6f;

    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        float exc = 0.0f;
        if (s->ex_n < s->ex_len) {
            float imp = s->ex_n == 0 ? s->amp : 0.0f;
            s->lp1 += s->lpk * (imp - s->lp1);
            s->lp2 += s->lpk * (s->lp1 - s->lp2);
            s->il += s->ik * (s->lp2 - s->il);
            float p = s->il, b = 0.0f;
            for (int i = 0; i < 4; i++) {
                float y = s->res_c[i] * s->res_y1[i] - s->res_r2[i] * s->res_y2[i] + s->res_g[i] * (p - s->p2);
                s->res_y2[i] = s->res_y1[i];
                s->res_y1[i] = y;
                b += y;
            }
            s->p2 = s->p1;
            s->p1 = p;
            float e = p + b;
            s->hist[s->hw & HIST_MASK] = e;
            int k0 = (int)s->comb_d;
            float fr = s->comb_d - (float)k0;
            float d0 = s->hist[(s->hw - k0) & HIST_MASK], d1 = s->hist[(s->hw - k0 - 1) & HIST_MASK];
            exc = (e - (d0 + fr * (d1 - d0))) * s->comb_norm;
            s->hw++;
            s->ex_n++;
        }
        if (bending) {
            s->bend += (bend_to - s->bend) * bk;
            s->D = fmaxf(2.0f, s->P / s->bend - s->tau);
        }
        float y = lagrange(s->line, LINE_MASK, s->w, s->D);
        s->lz += (1.0f - a) * (y + exc - s->lz);
        g *= g_mul;
        float t = g * s->lz;
        if (ac != 0.0f) {
            float ap = ac * t + s->ax1 - ac * s->ay1;
            s->ax1 = t;
            s->ay1 = ap;
            t = ap;
        }
        s->line[s->w] = t;
        s->w = (s->w + 1) & LINE_MASK;

        float knock = 0.0f;
        if (s->nz_env > 1e-4f) {
            knock = tint(&s->nz_t, noise(&s->rng)) * s->nz_env * s->nz_a;
            s->nz_env *= s->nz_td;
        }
        float out = y * s->level * v->fade;
        knock *= v->fade;
        if (v->fade_step > 0.0f) v->fade = fmaxf(0.0f, v->fade - v->fade_step);
        float m = fabsf(out);
        if (m > peak) peak = m;
        left[n] += (out + knock * 0.71f) * s->pan_l;
        right[n] += (out + knock * 0.71f) * s->pan_r;
        bridge[n] += out;
    }
    if (!bending) s->bend = bend_to;
    s->g_was = g_to;
    v->peak = peak;
    if ((peak < 1e-5f && s->ex_n >= s->ex_len && s->nz_env <= 1e-4f) ||
        (v->fade_step > 0.0f && v->fade <= 0.0f))
        v->active = 0;
}

/* ---- BLOOM: the open strings, shared by every note ---- */

static void bank_tune(quilt_t *q, wg_bank_t *b, int inst) {
    const wg_recipe_t *r = recipe_for(inst);
    memset(b, 0, sizeof(*b));
    b->inst = inst;
    const float dm = powf(2.0f, (q->g[G_DECAY] - 0.5f) * 4.0f);
    for (int i = 0; i < WG_BANK_STRINGS && r->symp[i]; i++) {
        wg_bank_string_t *s = &b->s[i];
        float f = 440.0f * powf(2.0f, (float)(r->symp[i] - 69) / 12.0f), w = TWO_PI * f / QUILT_SR;
        float t60 = 6.0f * dm * powf(f / 130.8f, -0.4f);
        loss(2500.0f, t60, f, &s->a, &s->g);
        s->D = QUILT_SR / f - lp_delay(s->a, w);
        b->n = i + 1;
    }
    b->decay = q->g[G_DECAY];
}

void waveguide_bank_render(quilt_t *q, const float *bridge, float *left, float *right, int frames) {
    wg_bank_t *b = &q->symp;
    const wg_recipe_t *r = recipe_for(q->type);
    const int want = r && r->ch == WC_BLOOM ? q->type : -1;
    float in_peak = 0.0f;
    for (int n = 0; n < frames; n++) in_peak = fmaxf(in_peak, fabsf(bridge[n]));

    /* A different instrument, or DECAY moved: retune once the bank is quiet,
     * or after fading what rings out over a block. */
    if (want >= 0 && (b->inst != want || fabsf(b->decay - q->g[G_DECAY]) > 0.02f)) {
        if (!b->active || b->fade <= 0.0f) {
            bank_tune(q, b, want);
            b->fade = 1.0f;
        } else {
            b->fading = 1;
        }
    }
    if (!b->active && (want < 0 || in_peak < 1e-7f)) return;
    if (b->inst < 0 || b->n == 0) return;

    const float bloom = q->charv[b->inst];
    const float cpl = want == b->inst && !b->fading ? 0.03f * bloom : 0.0f;
    const float fade_step = b->fading ? 1.0f / (float)frames : 0.0f;
    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        const float x = bridge[n] * cpl;
        float l = 0.0f, rr = 0.0f;
        for (int i = 0; i < b->n; i++) {
            wg_bank_string_t *s = &b->s[i];
            float y = lagrange(s->line, BANK_MASK, s->w, s->D);
            s->lz += (1.0f - s->a) * (y - s->lz);
            s->line[s->w] = s->g * s->lz + x;
            s->w = (s->w + 1) & BANK_MASK;
            if (i & 1) rr += y; else l += y;
        }
        l *= b->fade;
        rr *= b->fade;
        if (fade_step > 0.0f) b->fade = fmaxf(0.0f, b->fade - fade_step);
        peak = fmaxf(peak, fabsf(l) + fabsf(rr));
        left[n] += (0.75f * l + 0.25f * rr) * 1.5f;
        right[n] += (0.25f * l + 0.75f * rr) * 1.5f;
    }
    b->active = peak > 1e-6f || cpl > 0.0f;
    if (b->fading && b->fade <= 0.0f) {
        b->fading = 0;
        b->active = 0;
    }
}

void waveguide_bank_reset(wg_bank_t *b) {
    memset(b, 0, sizeof(*b));
    b->inst = -1;
}

/* ---- the bow: Solo Cello, Solo Violin, String Section ----
 *
 * Smith's bowed string: the string is two delay lines, the bow between
 * them at beta of the length from the bridge (VEIL), the nut reflecting
 * without loss and the bridge through the loss filter. Where they meet the
 * bow and the string stick or slip, after McIntyre, Schumacher & Woodhouse:
 * the bow gives the string the difference in velocity times a friction
 * curve, (|slope dv| + 0.75)^-4, near 1 while they stick and falling as
 * they slip; a firmer bow is a lower slope. That alone settles into
 * Helmholtz motion, one slip per period.
 *
 * Pad pressure is the bow: how hard it presses and how fast it moves. With
 * no pressure (PRESS on Auto, or none arriving) an automatic swell, its
 * height the key's velocity and its time SWELL, stands in. Lifted, the bow
 * leaves the string ringing with DECAY's release. The bow hair's width
 * (SOFT) rounds the slip. The body is after the voices, shared, because it
 * is linear (bowed_body). */

typedef enum { BC_VEIL, BC_WIDTH } bow_char_t;

typedef struct {
    const char *name;
    float gain;
    int lowest;
    float t60_ref, f_ref, t60_slope;   /* the string's own ring while bowed */
    float fb_mult;
    float body_hz[4], body_t60[4], body_g[4];
    int players;
    bow_char_t ch;
} bow_recipe_t;

static const bow_recipe_t BOW_RECIPES[] = {
    /* The cello's air mode near 100 Hz, its first wood modes either side of
     * 200 Hz, and the bridge's broad hill. */
    { .name = "Solo Cello", .gain = 1.02f, .lowest = 36,
      .t60_ref = 3.0f, .f_ref = 130.8f, .t60_slope = 0.5f, .fb_mult = 0.8f,
      .body_hz = { 105, 185, 220, 1600 }, .body_t60 = { 0.10f, 0.06f, 0.06f, 0.006f },
      .body_g = { 0.8f, 0.7f, 0.7f, 0.5f }, .players = 1, .ch = BC_VEIL },
    /* The violin's: air at 280 Hz, the main wood modes near 500, the
     * bridge hill at 2.5 kHz. */
    { .name = "Solo Violin", .gain = 0.915f, .lowest = 55,
      .t60_ref = 2.0f, .f_ref = 392.0f, .t60_slope = 0.5f, .fb_mult = 1.0f,
      .body_hz = { 280, 460, 530, 2500 }, .body_t60 = { 0.08f, 0.05f, 0.05f, 0.005f },
      .body_g = { 0.8f, 0.7f, 0.7f, 0.6f }, .players = 1, .ch = BC_VEIL },
    /* Three players a note, through one hall body between the cello's and
     * the violin's. */
    { .name = "String Section", .gain = 1.05f, .lowest = 36,
      .t60_ref = 2.5f, .f_ref = 196.0f, .t60_slope = 0.5f, .fb_mult = 0.9f,
      .body_hz = { 150, 300, 500, 2000 }, .body_t60 = { 0.08f, 0.05f, 0.05f, 0.006f },
      .body_g = { 0.7f, 0.6f, 0.6f, 0.5f }, .players = 3, .ch = BC_WIDTH },
};

static const bow_recipe_t *bow_recipe(int inst) {
    for (int i = 0; i < N(BOW_RECIPES); i++)
        if (!strcmp(BOW_RECIPES[i].name, QUILT_INST[inst].name)) return &BOW_RECIPES[i];
    return NULL;
}

int bowed_supports(int inst) { return bow_recipe(inst) != NULL; }

float bowed_freq(int inst, int note) {
    const bow_recipe_t *r = bow_recipe(inst);
    if (r) while (note < r->lowest) note += 12;
    while (note > 103) note -= 12;
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

void bowed_note_on(quilt_t *q, voice_t *v) {
    const int inst = v->inst;
    const bow_recipe_t *r = bow_recipe(inst);
    bow_voice_t *b = &v->bow;
    memset(b, 0, sizeof(*b));
    b->rng = 0x68E31DA4u ^ (uint32_t)(v->note * 7919 + (int)(v->vel * 1000.0f));
    const float f0 = bowed_freq(inst, v->note), w0 = TWO_PI * f0 / QUILT_SR;
    const float charv = q->charv[inst];
    const float soft = q->g[G_SOFT];
    b->f0 = f0;
    /* The hair's hiss, where the string's own overtones are. */
    tint_set(&b->nz_t, fminf(fmaxf(3.0f * f0, 500.0f), 2000.0f), 0.7f);
    b->P = QUILT_SR / f0;

    /* VEIL: the bow from near the bridge (bright, it takes a firm bow) to
     * over the fingerboard (sul tasto, flautando). Nearer the bridge the
     * string swings less for the same bow, so the level is evened out. */
    const float veil = r->ch == BC_VEIL ? charv : 0.5f;
    b->beta = 0.06f + 0.12f * veil;
    b->veil_comp = powf(b->beta / 0.12f, 0.5f);

    const float edge = slot(q, inst, "b_edge", 0.4f);
    const float t60 = r->t60_ref * powf(f0 / r->f_ref, -r->t60_slope);
    const float fb = 300.0f * powf(8000.0f / 300.0f, edge) * r->fb_mult;
    loss(fb, t60, f0, &b->a, &b->g_play);
    b->fb = fb;
    b->t60 = t60;
    b->cw = cosf(w0);
    b->budget = 60.0f / (t60 * f0);
    b->amax = (float)pole_cap(b->budget, b->cw);
    /* Lifted, the string rings on for DECAY: 0.1 s to 4 s. */
    const float rel = 0.1f * powf(40.0f, q->g[G_DECAY]);
    loss(fb, rel, f0, &b->a_rel, &b->g_rel);
    b->g = b->g_play;
    /* The bow drags the string along, which the loop of velocity waves
     * holds as a steady part. Bowing needs it; lifted, a string fixed at
     * both ends cannot keep it, so a DC blocker at a sixteenth of f0 fades
     * in at the bridge, or it would linger for seconds. */
    b->dc_r = 1.0f - TWO_PI * f0 * 0.0625f / QUILT_SR;
    b->tau = lp_delay(b->a, 1.15f * w0);
    /* The bridge side is fixed: the vibrato moves the finger, on the neck
     * side, and the bow stays where it is, so a two-point read will do. Not
     * whole samples: on a high note that would move the bow a percent or
     * more, onto places where the octave's motion is the steadier. */
    {
        const float db = fmaxf(1.0f, b->beta * b->P);
        b->Db = (int)db;
        b->Db_frac = db - (float)b->Db;
    }

    /* The swell: up to the key's velocity in SWELL's time, 20 ms to 1.2 s. */
    const float swell = slot(q, inst, "b_swell", 0.3f);
    b->lvl = 0.35f + 0.65f * v->vel;
    b->att_k = 1.0f - expf(-1.0f / (0.02f * powf(60.0f, swell) * QUILT_SR));
    b->rel_k = 1.0f - expf(-1.0f / (0.03f * QUILT_SR));
    /* BITE: the first grip, a firmer bow for the first 80 ms. */
    b->bite = slot(q, inst, "b_bite", 0.3f) * (0.3f + 0.7f * v->vel) * 1.5f;

    /* SOFT: the hair, from taut and narrow to slack and wide: a wider bow
     * rounds the slip (a low-pass on what it gives the string) and grips
     * less. */
    const float hair = fmaxf(12000.0f * powf(0.5f, soft), 8.0f * f0);
    b->hair_k = 1.0f - expf(-TWO_PI * fminf(hair, 0.45f * QUILT_SR) / QUILT_SR);
    /* Near the bridge a bow must press harder to hold the string
     * (Schelleng); over the fingerboard it keeps its weight, or it slips
     * twice a period and the note jumps an octave. */
    b->force_mul = sqrtf(fmaxf(1.0f, 0.12f / b->beta));

    b->level = r->gain;
    /* The players: one, or three a little apart in pitch, place and time
     * (WIDTH), each with a vibrato of its own. Even together they are a
     * couple of cents apart, as players always are, so they never add up
     * in phase. */
    b->n = r->players;
    const float width = r->ch == BC_WIDTH ? charv : 0.0f;
    const float pan0 = 0.5f + 0.35f * (float)(v->note - 64) / 40.0f;
    for (int i = 0; i < b->n; i++) {
        bow_string_t *s = &b->s[i];
        float k = b->n > 1 ? (float)(i - 1) : 0.0f;
        float cents = k * (2.0f + 20.0f * width) + noise(&b->rng) * 0.5f;
        s->detune = powf(2.0f, cents / 1200.0f);
        float pan = fminf(fmaxf(pan0 + k * 0.35f * width, 0.0f), 1.0f);
        s->pan_l = cosf(pan * PI * 0.5f) / sqrtf((float)b->n);
        s->pan_r = sinf(pan * PI * 0.5f) / sqrtf((float)b->n);
        s->vib_ph = 0.5f + 0.5f * noise(&b->rng);
        s->vib_rate = 1.0f + 0.07f * k + 0.02f * noise(&b->rng);
        s->onset = (b->n > 1 ? (float)i * 0.007f * width : 0.0f);
        s->Pt = b->P / s->detune;
    }
}

void bowed_render(quilt_t *q, voice_t *v, float *left, float *right, int frames) {
    bow_voice_t *b = &v->bow;
    const int inst = v->inst;
    const float dt = 1.0f / QUILT_SR;
    const int press_src = (int)slot(q, inst, "b_press", 0.0f);   /* Pad, Auto, Blend */
    const float noise_a = noise_amount(slot(q, inst, "b_noise", 0.2f), 7.4f);
    /* SWAY (or the mod wheel) is the vibrato, up to 40 cents, at SPEED; it
     * comes in after the note has spoken. */
    const float depth = fmaxf(q->gs[G_SWAY], q->modwheel) * 40.0f / 1200.0f * 0.6931f;
    const float vib_hz = 0.5f + 7.5f * q->gs[G_SPEED];
    const float pk = 1.0f - expf(-1.0f / (0.012f * QUILT_SR));
    /* The force rounds the corners of the motion (Cremer): a lighter bow,
     * over the fingerboard (VEIL) or with slack hair (SOFT), a rounder
     * corner reaching the bridge, so a darker note. The rounding is the
     * corner's, once, as it reaches the bridge, so it is a smoothing of
     * what the bridge hears, not a loss in the loop, where it would fight
     * the bow for the motion. Worked out once a block. */
    float rk;
    {
        float p0 = b->env;
        if (v->got_press && press_src != 1)
            p0 = press_src == 0 ? fmaxf(b->press_s, 0.25f * b->env) : 0.5f * (b->env + b->press_s);
        const float f = fminf(1.0f, fmaxf(0.55f, (0.25f + 0.75f * p0) * b->force_mul + b->bite * expf(-b->t * 12.5f)));
        float fc = b->f0 * 30.0f * powf(f / 0.6f, 2.5f) * powf(0.12f / b->beta, 2.0f) * powf(0.025f, q->gs[G_SOFT]);
        fc = fminf(fmaxf(fc, 1.5f * b->f0), 0.45f * QUILT_SR);
        rk = 1.0f - expf(-TWO_PI * fc / QUILT_SR);
        if (!v->held) b->a = b->a_rel;
    }
    const float g_to = v->held ? b->g_play : b->g_rel;

    /* The vibrato's period for each player at the end of this block; the
     * read ramps to it from the last, so the sine is worked out once a
     * block. */
    const float bt1 = b->t + (float)frames * dt;
    b->vib_in = fminf(1.0f, fmaxf(0.0f, (bt1 - 0.25f) * 2.5f));
    float p_from[BOW_PLAYERS], p_step[BOW_PLAYERS];
    for (int i = 0; i < b->n; i++) {
        bow_string_t *s = &b->s[i];
        s->vib_ph += vib_hz * s->vib_rate * (float)frames * dt;
        s->vib_ph -= (float)(int)s->vib_ph;
        float to = b->P / (s->detune * (1.0f + depth * b->vib_in * fast_sin(s->vib_ph)));
        p_from[i] = s->Pt;
        p_step[i] = (to - s->Pt) / (float)frames;
        s->Pt = to;
    }
    float bite = b->bite * expf(-b->t * 12.5f);
    const float bite_k = expf(-12.5f * dt);
    const int Db = b->Db;
    const float dbf = b->Db_frac, Dbr = (float)Db + dbf;
    /* The neck's read moves slowly with the vibrato, so its four weights
     * are worked out at both ends of the block and stepped between, unless
     * it crosses a whole sample on the way. */
    int iD[BOW_PLAYERS], ramp[BOW_PLAYERS];
    float h[BOW_PLAYERS][4], hs[BOW_PLAYERS][4];
    for (int i = 0; i < b->n; i++) {
        float d0 = fmaxf(2.0f, p_from[i] + p_step[i] - Dbr - b->tau);
        float d1 = fmaxf(2.0f, p_from[i] + p_step[i] * (float)frames - Dbr - b->tau);
        iD[i] = (int)d0;
        ramp[i] = (int)d1 == iD[i];
        if (ramp[i]) {
            float h1[4];
            lagrange_h(d0 - (float)iD[i] + 1.0f, h[i]);
            lagrange_h(d1 - (float)iD[i] + 1.0f, h1);
            for (int k = 0; k < 4; k++) hs[i][k] = frames > 1 ? (h1[k] - h[i][k]) / (float)(frames - 1) : 0.0f;
        }
    }
    const float dcm = b->dc_mix;
    const int np = b->n;
    /* What changes sample by sample, kept out of memory: the strings'
     * writes could otherwise be any of it. */
    const int held = v->held;
    const float press = v->press, fade_step = v->fade_step, one_a = 1.0f - b->a;
    const float gain0 = b->level * b->veil_comp * 0.1f;
    float env = b->env, press_s = b->press_s, g = b->g, lift = b->lift, dc_mix = b->dc_mix, bt = b->t, fade = v->fade;
    tint_t nzt = b->nz_t;
    uint32_t rng = b->rng;
    struct { float lz, hz, dcx, dcy, r1, r2, pl, pr, onset; } st[BOW_PLAYERS];
    for (int i = 0; i < np; i++) {
        const bow_string_t *s = &b->s[i];
        st[i].lz = s->lz, st[i].hz = s->hz, st[i].dcx = s->dcx, st[i].dcy = s->dcy, st[i].r1 = s->r1, st[i].r2 = s->r2;
        st[i].pl = s->pan_l, st[i].pr = s->pan_r, st[i].onset = s->onset;
    }
    const int use_press = v->got_press && press_src != 1;
    const float env_to = held ? b->lvl : 0.0f, env_k = held ? b->att_k : b->rel_k;
    const float force_mul = b->force_mul, tau = b->tau, dc_r = b->dc_r, hair_k = b->hair_k;
    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        /* The bow's pressure, 0..1. */
        env += (env_to - env) * env_k;
        float p = env;
        if (use_press) {
            press_s += ((held ? press : 0.0f) - press_s) * pk;
            /* Pad: the pad, with a quarter of the swell under it, so a held
             * key never falls silent; Blend, half each. */
            p = press_src == 0 ? fmaxf(press_s, 0.25f * env) : 0.5f * (env + press_s);
        }
        g += (g_to - g) * 0.002f;
        bite *= bite_k;
        const float base = (0.25f + 0.75f * p) * force_mul;
        const float force = fminf(1.0f, fmaxf(0.55f, base + bite));
        const float slope = 5.0f - 4.0f * force;
        /* Released, the bow leaves the string in about 15 ms and lets it
         * ring; slowing on it, it would damp it. */
        lift += ((held ? 1.0f : 0.0f) - lift) * (held ? 1.0f : 0.0015f);
        const float contact = fminf(1.0f, p * 6.0f) * lift;
        dc_mix += ((held ? 0.0f : 1.0f) - dc_mix) * (held ? 1.0f : 0.0005f);
        float vb = 0.22f * p * (1.0f - 0.5f * bite);
        /* NOISE: the hair on the string, a hiss that follows the bow. */
        const float hiss = tint(&nzt, noise(&rng)) * noise_a * p * contact * 0.15f;

        float l = hiss * 0.7f, rr = hiss * 0.7f;
        const float cbow = contact;
        for (int i = 0; i < np; i++) {
            bow_string_t *s = &b->s[i];
            float b0 = s->bridge[(s->wb - Db) & (BOW_BRIDGE - 1)], b1 = s->bridge[(s->wb - Db - 1) & (BOW_BRIDGE - 1)];
            float ob = b0 + (b1 - b0) * dbf;
            float on;
            if (ramp[i]) {
                const int w = s->wn, id = iD[i], m = BOW_NECK - 1;
                on = h[i][0] * s->neck[(w - id + 1) & m] + h[i][1] * s->neck[(w - id) & m] +
                     h[i][2] * s->neck[(w - id - 1) & m] + h[i][3] * s->neck[(w - id - 2) & m];
                for (int k = 0; k < 4; k++) h[i][k] += hs[i][k];
            } else {
                on = lagrange(s->neck, BOW_NECK - 1, s->wn, fmaxf(2.0f, p_from[i] + p_step[i] * (float)(n + 1) - Dbr - tau));
            }
            float lz = st[i].lz += one_a * (ob - st[i].lz);
            if (dcm > 0.0f) {
                float dc = lz - st[i].dcx + dc_r * st[i].dcy;
                st[i].dcx = lz;
                st[i].dcy = dc;
                lz += (dc - lz) * dcm;
            }
            float br = -g * lz, nr = -on;
            float dv = vb - (br + nr);
            float x = fabsf(dv * slope) + 0.75f, t = 1.0f / (x * x);
            float fr = fminf(1.0f, t * t);
            float c = bt >= st[i].onset ? cbow : 0.0f;
            st[i].hz += (dv * fr * c - st[i].hz) * hair_k;
            s->neck[s->wn] = br + st[i].hz;
            s->bridge[s->wb] = nr + st[i].hz;
            s->wn = (s->wn + 1) & (BOW_NECK - 1);
            s->wb = (s->wb + 1) & (BOW_BRIDGE - 1);
            st[i].r1 += (ob - st[i].r1) * rk;
            st[i].r2 += (st[i].r1 - st[i].r2) * rk;
            l += st[i].r2 * st[i].pl;
            rr += st[i].r2 * st[i].pr;
        }
        const float gain = gain0 * fade;
        l *= gain;
        rr *= gain;
        if (fade_step > 0.0f) fade = fmaxf(0.0f, fade - fade_step);
        float m = fmaxf(fabsf(l), fabsf(rr));
        if (m > peak) peak = m;
        left[n] += l;
        right[n] += rr;
        bt += dt;
    }
    b->env = env, b->press_s = press_s, b->g = g, b->lift = lift, b->dc_mix = dc_mix, b->t = bt, v->fade = fade;
    b->nz_t = nzt;
    b->rng = rng;
    for (int i = 0; i < np; i++) {
        bow_string_t *s = &b->s[i];
        s->lz = st[i].lz, s->hz = st[i].hz, s->dcx = st[i].dcx, s->dcy = st[i].dcy, s->r1 = st[i].r1, s->r2 = st[i].r2;
    }
    v->peak = peak;
    if ((!v->held && b->env < 1e-4f && peak < 1e-5f) || (v->fade_step > 0.0f && v->fade <= 0.0f))
        v->active = 0;
}

/* The bowed strings' body: the direct path plus four band-passes, each up to
 * about 12 dB at BODY full, for the last bowed instrument chosen, once for
 * every note. */
void bowed_body(quilt_t *q, const float *bl, const float *br, float *left, float *right, int frames) {
    if (bowed_supports(q->type)) q->bowbody.inst = q->type;
    const int inst = q->bowbody.inst;
    const bow_recipe_t *r = inst >= 0 ? bow_recipe(inst) : NULL;
    if (!r) return;
    const float body = slot(q, inst, "b_body", 0.6f);
    float c[4], r2[4], g[4];
    for (int i = 0; i < 4; i++) {
        float w = TWO_PI * r->body_hz[i] / QUILT_SR, rr = expf(-6.91f / (r->body_t60[i] * QUILT_SR));
        c[i] = 2.0f * rr * cosf(w);
        r2[i] = rr * rr;
        g[i] = 4.0f * r->body_g[i] * body * (1.0f - rr);
    }
    for (int ch = 0; ch < 2; ch++) {
        const float *in = ch ? br : bl;
        float *out = ch ? right : left;
        float (*z)[2] = q->bowbody.z[ch], *x = q->bowbody.x[ch];
        for (int n = 0; n < frames; n++) {
            float p = in[n], y = p;
            for (int i = 0; i < 4; i++) {
                float o = c[i] * z[i][0] - r2[i] * z[i][1] + g[i] * (p - x[1]);
                z[i][1] = z[i][0];
                z[i][0] = o;
                y += o;
            }
            x[1] = x[0];
            x[0] = p;
            /* and nothing steady reaches the output */
            float d = y - q->bowbody.dcx[ch] + 0.9993f * q->bowbody.dcy[ch];
            q->bowbody.dcx[ch] = y;
            q->bowbody.dcy[ch] = d;
            out[n] += d;
        }
    }
}
