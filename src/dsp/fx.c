/*
 * Shared effects. DESIGN.md, Shared effects. In order:
 *
 *   soundboard   pianos only: the voices' BODY send through a bank of the
 *                board's broad resonances, mixed back in. (The commuted
 *                soundboard excitation of Smith & Van Duyne is a later
 *                refinement; this is its linear, shared half.)
 *   tremolo      SWAY's auto-pan, at SPEED, for the instruments whose SWAY is
 *                a tremolo (on the vibraphone it is the fan, in modal.c). The
 *                reed piano's is in one channel, as its amplifier's is.
 *   drive        DRIVE: gentle saturation, transparent when quiet.
 *   leslie       the organ only: horn and drum, each a Doppler delay with
 *                its own swing in level and tone, heard by two microphones;
 *                SWAY moves both rotors from SLOW to FAST, and they get
 *                there with their own inertia.
 *   plate        Dattorro's plate (JAES 1997), SPACE as its send; SIZE its
 *                decay, DARK its damping, DELAY its pre-delay.
 *   tilt         TONE, two shelves, about 600 Hz and 3 kHz.
 *
 * Knob values glide (voice.c) and ramp across each block here, so turning a
 * knob never steps the signal. Volume and the limiter follow in quilt.c.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
/* Dattorro's delays are given at 29761 Hz. */
#define D(n) ((int)((n) * (44100.0f / 29761.0f) + 0.5f))

void fx_reset(fx_t *fx) { memset(fx, 0, sizeof(*fx)); }

static const float BOARD_HZ[8] = { 95, 170, 280, 420, 650, 1000, 1700, 2800 };
static const float BOARD_GAIN[8] = { 0.9f, 1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.45f, 0.3f };

static void soundboard(fx_t *fx, float *left, float *right, const float *board, int frames) {
    float b0[8], a1[8], a2[8];
    for (int i = 0; i < 8; i++) {
        /* RBJ band-pass, 0 dB peak, Q 3. */
        float w = TWO_PI * BOARD_HZ[i] / QUILT_SR, alpha = sinf(w) / (2.0f * 3.0f), a0 = 1.0f + alpha;
        b0[i] = alpha / a0 * BOARD_GAIN[i];
        a1[i] = -2.0f * cosf(w) / a0;
        a2[i] = (1.0f - alpha) / a0;
    }
    for (int n = 0; n < frames; n++) {
        float x = board[n], y = 0.0f;
        for (int i = 0; i < 8; i++) {
            /* Transposed direct form II; b1 = 0, b2 = -b0. */
            float out = b0[i] * x + fx->board_z[i][0];
            fx->board_z[i][0] = -a1[i] * out + fx->board_z[i][1];
            fx->board_z[i][1] = -b0[i] * x - a2[i] * out;
            y += out;
        }
        left[n] += y * 0.8f;
        right[n] += y * 0.8f;
    }
}

static inline float allpass(float *buf, int mask, int w, int delay, float g, float x) {
    float d = buf[(w - delay) & mask];
    float v = x - g * d;
    buf[w & mask] = v;
    return d + g * v;
}

static inline float allpass_mod(float *buf, int mask, int w, float delay, float g, float x) {
    int i = (int)delay;
    float frac = delay - (float)i;
    float d = buf[(w - i) & mask] * (1.0f - frac) + buf[(w - i - 1) & mask] * frac;
    float v = x - g * d;
    buf[w & mask] = v;
    return d + g * v;
}

static inline float tap(const float *buf, int mask, int w, int k) { return buf[(w - k) & mask]; }

/* The Leslie (Smith, Serafin, Abel & Berners; Herrera, Hanson & Abel).
 * Below 800 Hz the drum, above it the horn. Each rotor's speed chases SWAY's
 * place between its slow and fast speeds (or the mod wheel's, if further
 * up), the light horn in under a second, the heavy drum in about five, and
 * slower to stop than to start. As the
 * horn turns, its sound comes nearer and goes (a delay swinging about half a
 * millisecond), louder and brighter facing the microphone. */
static void leslie(quilt_t *q, float *left, float *right, int frames) {
    fx_t *fx = &q->fx;
    const int inst = q->type;
    const float slow = q->slot[inst][quilt_shape_key_in(SH_ORGAN, "o_slow")];
    const float fast = q->slot[inst][quilt_shape_key_in(SH_ORGAN, "o_fast")];
    const float sway = fmaxf(q->gs[G_SWAY], q->modwheel);   /* the mod wheel works it too */
    const float hs = 0.4f + 1.2f * slow, hf = 5.0f + 3.0f * fast;
    const float th = hs + (hf - hs) * sway, td = (hs + (hf - hs) * sway) * 0.84f;
    const float dt = (float)frames / QUILT_SR;
    fx->horn_hz += (th - fx->horn_hz) * (1.0f - expf(-dt / (th > fx->horn_hz ? 0.7f : 0.9f)));
    fx->drum_hz += (td - fx->drum_hz) * (1.0f - expf(-dt / (td > fx->drum_hz ? 4.5f : 5.5f)));
    const float xo = 1.0f - expf(-TWO_PI * 800.0f / QUILT_SR);
    const float hinc = fx->horn_hz / QUILT_SR, dinc = fx->drum_hz / QUILT_SR;
    const float kdull = 1.0f - expf(-TWO_PI * 2500.0f / QUILT_SR), kopen = 1.0f - expf(-TWO_PI * 12000.0f / QUILT_SR);
    static const float mic[2] = { 0.0f, 0.3f };   /* where the two microphones stand, in turns */
    for (int n = 0; n < frames; n++) {
        const float x = 0.5f * (left[n] + right[n]);
        fx->les_xo[0] += (x - fx->les_xo[0]) * xo;
        fx->les_xo[1] += (fx->les_xo[0] - fx->les_xo[1]) * xo;
        fx->les_drum[fx->les_w & 255] = fx->les_xo[1];
        fx->les_horn[fx->les_w & 255] = x - fx->les_xo[1];
        fx->horn_ang += hinc;
        if (fx->horn_ang >= 1.0f) fx->horn_ang -= 1.0f;
        fx->drum_ang += dinc;
        if (fx->drum_ang >= 1.0f) fx->drum_ang -= 1.0f;
        float out[2];
        for (int m = 0; m < 2; m++) {
            const float ha = fx->horn_ang + mic[m], da = fx->drum_ang + mic[m];
            const float hs_ = fast_sin(ha), hc = fast_sin(ha + 0.25f);
            const float ds_ = fast_sin(da), dc = fast_sin(da + 0.25f);
            float d = 40.0f + 22.0f * hs_, fl = floorf(d), fr = d - fl;
            int i0 = fx->les_w - (int)fl;
            float h = fx->les_horn[i0 & 255] * (1.0f - fr) + fx->les_horn[(i0 - 1) & 255] * fr;
            h *= 0.7f + 0.3f * hc;
            fx->les_hlp[m] += (h - fx->les_hlp[m]) * (kdull + (kopen - kdull) * (0.5f + 0.5f * hc));
            d = 40.0f + 6.0f * ds_; fl = floorf(d); fr = d - fl;
            i0 = fx->les_w - (int)fl;
            float b = fx->les_drum[i0 & 255] * (1.0f - fr) + fx->les_drum[(i0 - 1) & 255] * fr;
            out[m] = fx->les_hlp[m] + b * (0.85f + 0.15f * dc);
        }
        fx->les_w++;
        left[n] = out[0];
        right[n] = out[1];
    }
}

/* The send: SPACE. Full up sends 1.75 times the dry sound into the plate. */
static float plate_send(const float *g) { return g[G_SPACE] * 1.75f; }

static void plate(quilt_t *q, float *left, float *right, int frames) {
    fx_t *fx = &q->fx;
    const float send0 = plate_send(q->gs_prev), send1 = plate_send(q->gs);
    if (send0 < 1e-4f && send1 < 1e-4f && fx->fb[0] == 0.0f && fx->fb[1] == 0.0f) return;
    /* SIZE, DARK and DELAY ramp across the block like the rest. DARK sets the
     * tank's damping as a cutoff, 9 kHz open to 400 Hz muffled, so it is
     * heard on soft instruments, which have little above a kilohertz. */
    const float dec0 = 0.2f + 0.73f * q->gs_prev[G_SIZE], dec1 = 0.2f + 0.73f * q->gs[G_SIZE];
    const float k0 = 1.0f - expf(-TWO_PI * 9000.0f * powf(400.0f / 9000.0f, q->gs_prev[G_DARK]) / QUILT_SR);
    const float k1 = 1.0f - expf(-TWO_PI * 9000.0f * powf(400.0f / 9000.0f, q->gs[G_DARK]) / QUILT_SR);
    /* DELAY glides slower, over about 300 ms, so the tail bends gently. */
    const float pre0 = fx->pre_s;
    fx->pre_s += (1.0f + q->g[G_DELAY] * 0.1f * QUILT_SR - fx->pre_s) * (1.0f - expf(-(float)frames / (0.3f * QUILT_SR)));
    const float pre1 = fx->pre_s;
    const float step = 1.0f / (float)frames;
    const float exc = 16.0f * (44100.0f / 29761.0f);
    const float lfo_cr = cosf(TWO_PI * 0.8f / QUILT_SR), lfo_sr = sinf(TWO_PI * 0.8f / QUILT_SR);
    float quiet = 0.0f;
    if (fx->lfo == 0.0f) fx->lfo = 1.0f;     /* the tank LFO is a phasor: cos in lfo, sin in lfo_s */

    for (int n = 0; n < frames; n++) {
        const int w = fx->w++;
        const float t = (float)(n + 1) * step;
        const float send = send0 + (send1 - send0) * t;
        const float decay = dec0 + (dec1 - dec0) * t, keep = k0 + (k1 - k0) * t;
        /* The pre-delay read glides between samples, so turning DELAY bends
         * the tail's pitch for a moment instead of jumping it. */
        const float pre = pre0 + (pre1 - pre0) * t;
        const int pi = (int)pre;
        const float pf = pre - (float)pi;
        fx->pre[w & 8191] = 0.5f * (left[n] + right[n]) * send;
        float x = fx->pre[(w - pi) & 8191] * (1.0f - pf) + fx->pre[(w - pi - 1) & 8191] * pf;
        fx->bw += 0.9995f * (x - fx->bw);
        x = allpass(fx->ap_in[0], 1023, w, D(142), 0.75f, fx->bw);
        x = allpass(fx->ap_in[1], 1023, w, D(107), 0.75f, x);
        x = allpass(fx->ap_in[2], 1023, w, D(379), 0.625f, x);
        x = allpass(fx->ap_in[3], 1023, w, D(277), 0.625f, x);

        float lc = fx->lfo * lfo_cr - fx->lfo_s * lfo_sr;
        fx->lfo_s = fx->lfo_s * lfo_cr + fx->lfo * lfo_sr;
        fx->lfo = lc;
        const float mod[2] = { exc * fx->lfo_s, exc * fx->lfo };
        static const int AP1[2] = { D(672), D(908) }, D1[2] = { D(4453), D(4217) };
        static const int AP2[2] = { D(1800), D(2656) }, D2[2] = { D(3720), D(3163) };
        float in[2] = { x + fx->fb[1], x + fx->fb[0] };
        for (int s = 0; s < 2; s++) {
            float a = allpass_mod(fx->tank_ap1[s], 2047, w, (float)AP1[s] + exc + mod[s], -0.70f, in[s]);
            fx->tank_d1[s][w & (PLATE_LEN - 1)] = a;
            float b = tap(fx->tank_d1[s], PLATE_LEN - 1, w, D1[s]);
            fx->damp_z[s] += keep * (b - fx->damp_z[s]);
            b = fx->damp_z[s] * decay;
            float c = allpass(fx->tank_ap2[s], 4095, w, AP2[s], 0.5f, b);
            fx->tank_d2[s][w & (PLATE_LEN - 1)] = c;
            fx->fb[s] = tap(fx->tank_d2[s], PLATE_LEN - 1, w, D2[s]) * decay;
        }

        const int M1 = PLATE_LEN - 1;
        float (*d1)[PLATE_LEN] = fx->tank_d1, (*d2)[PLATE_LEN] = fx->tank_d2;
        float (*ap)[4096] = fx->tank_ap2;
        float yl = tap(d1[1], M1, w, D(266)) + tap(d1[1], M1, w, D(2974)) - tap(ap[1], 4095, w, D(1913)) +
                   tap(d2[1], M1, w, D(1996)) - tap(d1[0], M1, w, D(1990)) - tap(ap[0], 4095, w, D(187)) -
                   tap(d2[0], M1, w, D(1066));
        float yr = tap(d1[0], M1, w, D(353)) + tap(d1[0], M1, w, D(3627)) - tap(ap[0], 4095, w, D(1228)) +
                   tap(d2[0], M1, w, D(2673)) - tap(d1[1], M1, w, D(2111)) - tap(ap[1], 4095, w, D(335)) -
                   tap(d2[1], M1, w, D(121));
        left[n] += 0.6f * yl;
        right[n] += 0.6f * yr;
        quiet += fabsf(fx->fb[0]) + fabsf(fx->fb[1]);
    }
    /* Keep the phasor on the unit circle. */
    float norm = 1.5f - 0.5f * (fx->lfo * fx->lfo + fx->lfo_s * fx->lfo_s);
    fx->lfo *= norm;
    fx->lfo_s *= norm;
    /* Let a silent tank go fully quiet, so it can be skipped. */
    if (send1 < 1e-4f && quiet < 1e-7f * (float)frames) fx->fb[0] = fx->fb[1] = 0.0f;
}

void fx_process(quilt_t *q, float *left, float *right, float *board, int frames) {
    fx_t *fx = &q->fx;
    soundboard(fx, left, right, board, frames);

    /* Everything below ramps from last block's knob values to this block's. */
    const float step = 1.0f / (float)frames;
    const float sway0 = q->gs_prev[G_SWAY], sway1 = q->gs[G_SWAY];
    const float inc = TWO_PI * (0.5f + 7.5f * q->gs[G_SPEED]) / QUILT_SR;
    const int trem = quilt_sway_is_tremolo(q->type) && (sway0 > 0.0f || sway1 > 0.0f);
    const int pan = quilt_sway_is_pan(q->type);
    for (int n = 0; n < frames; n++) {
        q->lfo += inc;
        if (q->lfo > TWO_PI) q->lfo -= TWO_PI;
        if (trem) {
            float sway = sway0 + (sway1 - sway0) * (float)(n + 1) * step;
            float s = 0.5f * sinf(q->lfo);
            left[n] *= 1.0f - sway * (0.5f + s);
            right[n] *= 1.0f - sway * (0.5f + (pan ? -s : s));
        }
    }

    const float drive0 = q->gs_prev[G_DRIVE], drive1 = q->gs[G_DRIVE];
    if (drive0 > 0.001f || drive1 > 0.001f) {
        for (int n = 0; n < frames; n++) {
            float drive = drive0 + (drive1 - drive0) * (float)(n + 1) * step;
            float g = 1.0f + 8.0f * drive * drive, makeup = (1.0f + drive) / g;
            left[n] = tanhf(left[n] * g) * makeup;
            right[n] = tanhf(right[n] * g) * makeup;
        }
    }

    if (QUILT_INST[q->type].shape == SH_ORGAN) leslie(q, left, right, frames);
    else fx->horn_hz = fx->drum_hz = 0.0f;   /* a Leslie switched on starts from rest */

    plate(q, left, right, frames);

    /* TONE tilts the whole range: one shelf about 600 Hz and a gentler one
     * about 3 kHz, so it is heard on a glockenspiel as well as a piano. At
     * the ends, about 10 dB either way across 600 Hz, and 8 dB more across
     * 3 kHz; the middle is flat. */
    const float db0 = (q->gs_prev[G_TONE] - 0.5f) * 20.0f;
    const float db1 = (q->gs[G_TONE] - 0.5f) * 20.0f;
    if (fabsf(db0) > 0.05f || fabsf(db1) > 0.05f) {
        static const float pivot[2] = { 600.0f, 3000.0f }, share[2] = { 1.0f, 0.8f };
        for (int st = 0; st < 2; st++) {
            const float gh0 = powf(10.0f, db0 * share[st] / 40.0f), gh1 = powf(10.0f, db1 * share[st] / 40.0f);
            const float k = 1.0f - expf(-TWO_PI * pivot[st] / QUILT_SR);
            float *z = fx->tilt_z[st];
            for (int n = 0; n < frames; n++) {
                float gh = gh0 + (gh1 - gh0) * (float)(n + 1) * step, gl = 1.0f / gh;
                z[0] += k * (left[n] - z[0]);
                z[1] += k * (right[n] - z[1]);
                left[n] = z[0] * gl + (left[n] - z[0]) * gh;
                right[n] = z[1] * gl + (right[n] - z[1]) * gh;
            }
        }
    }
}
