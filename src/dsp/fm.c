/*
 * The synthetic engine's FM voice: the Glass E.Piano. DESIGN.md, Synthetic.
 *
 * Chowning FM in two operator pairs, after the 1980s FM electric pianos:
 *
 *   Tine pair  carrier at f0, modulator at 14 f0. Its index jumps with the
 *              blow and falls within about 60 ms to a small floor, so a firm
 *              note opens with the bright, inharmonic "tink" (TINE) and
 *              settles to nearly a sine.
 *   Bell pair  carrier at f0, a little detuned (SPLIT), modulator at TUNE's
 *              ratio, 1 to 5: whole numbers ring as bells, the ratios
 *              between them as glass. Its index is GLASS; it rings shorter
 *              than the tine pair, so the note mellows as it sounds.
 *
 * Velocity drives both indices, so soft playing is nearly a sine. SOFT
 * scales them too and softens the attack. Phases are in cycles and the sine
 * is fast_sin (quilt.h); four per voice per sample.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

int fm_supports(int inst) { return !strcmp(QUILT_INST[inst].name, "Glass E.Piano"); }

static float slot(const quilt_t *q, int inst, const char *key, float fallback) {
    int i = quilt_shape_key_in(QUILT_INST[inst].shape, key);
    return i < 0 ? fallback : q->slot[inst][i];
}

/* Per-sample multiplier for a fall of 60 dB in t60 seconds. */
static float fall(float t60) { return expf(-6.91f / (t60 * QUILT_SR)); }

void fm_note_on(quilt_t *q, voice_t *v) {
    fm_voice_t *m = &v->fm;
    const int inst = v->inst;
    memset(m, 0, sizeof(*m));
    const float f0 = 440.0f * powf(2.0f, (float)(v->note - 69) / 12.0f);
    const float vel = v->vel;
    const float tune = slot(q, inst, "f_tune", 0.5f);
    const float tine = slot(q, inst, "f_tine", 0.4f);
    const float split = slot(q, inst, "f_split", 0.2f);
    const float glass = q->charv[inst];
    const float soft = q->g[G_SOFT];
    /* SOFT: the indices a decade either way of the middle, and an attack
     * from instant to about 8 ms. */
    const float hard = powf(10.0f, (0.5f - soft) * 1.0f);
    const float cents = 1.0f + 14.0f * split;

    m->inc[0] = f0 / QUILT_SR;
    m->inc[1] = 14.0f * f0 / QUILT_SR;
    m->inc[2] = f0 * powf(2.0f, cents / 1200.0f) / QUILT_SR;
    m->inc[3] = f0 * (1.0f + 4.0f * tune) / QUILT_SR;
    /* Start the pairs apart, so SPLIT's beating is not a phase cancellation. */
    m->ph[2] = 0.25f;

    /* Ringing: 5 s at middle C, shorter up the keys, DECAY a factor of 4 either way. */
    const float dm = powf(2.0f, (q->g[G_DECAY] - 0.5f) * 4.0f);
    const float t60 = 5.0f * powf(f0 / 261.6f, -0.6f) * dm;
    m->amp = 0.15f + 0.85f * vel;
    m->amp_k = fall(t60);
    m->amp_kd = fall(0.3f);
    /* Up to about 2.5 at full: beyond it the tink's sideband falls back
     * toward its first null, and turning up would turn it down. */
    m->ia = (0.03f + 3.2f * tine * vel * vel) * hard;
    m->ia_floor = 0.04f * hard;
    m->ia_k = fall(0.25f);
    m->ib = (0.1f + 2.4f * glass * (0.3f + 0.7f * vel)) * hard;
    m->ib_k = fall(t60 * 0.5f);
    m->bell = 0.6f;
    m->bell_k = fall(t60 * 0.6f);
    m->att = soft > 0.5f ? 0.0f : 1.0f;
    m->att_k = 1.0f - expf(-1.0f / (0.008f * (soft - 0.5f) * 2.0f * QUILT_SR + 1.0f));
    m->dampable = 1;
}

void fm_render(quilt_t *q, voice_t *v, float *left, float *right, int frames) {
    fm_voice_t *m = &v->fm;
    int damped = !v->held && !q->pedal;
    if (damped != m->damped) m->damped = damped;
    const float ak = damped ? m->amp_kd : m->amp_k;
    /* A little spread across the keys. */
    const float pan = 0.5f + 0.3f * (float)(v->note - 64) / 40.0f;
    const float pl = cosf(pan * 1.5707963f), pr = sinf(pan * 1.5707963f);
    const float level = 0.338f;
    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        float idx_a = m->ia_floor + m->ia;
        float mod_a = fast_sin(m->ph[1]) * idx_a * (1.0f / 6.28318530718f);
        float mod_b = fast_sin(m->ph[3]) * m->ib * (1.0f / 6.28318530718f);
        float y = fast_sin(m->ph[0] + mod_a) + m->bell * fast_sin(m->ph[2] + mod_b);
        m->att += (1.0f - m->att) * m->att_k;
        y *= m->amp * m->att * level * v->fade;
        for (int k = 0; k < 4; k++) {
            m->ph[k] += m->inc[k];
            if (m->ph[k] >= 1.0f) m->ph[k] -= 1.0f;
        }
        m->amp *= ak;
        m->ia *= m->ia_k;
        m->ib *= m->ib_k;
        m->bell *= m->bell_k;
        if (v->fade_step > 0.0f) v->fade = fmaxf(0.0f, v->fade - v->fade_step);
        float a = fabsf(y);
        if (a > peak) peak = a;
        left[n] += y * pl;
        right[n] += y * pr;
    }
    v->mv.peak = peak;
    if ((m->amp < 1e-5f) || (v->fade_step > 0.0f && v->fade <= 0.0f)) v->active = 0;
}
