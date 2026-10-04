/*
 * Voices: how a note is started and released, and how voices are shared out.
 *
 *   - Every instrument TYPE offers plays through its engine (modal.c).
 *   - Struck notes ring after release while a pedal holds them (CC64, or the
 *     modal page's PEDAL knob), and the damper stops them.
 *   - A global budget of modal oscillators, not a voice count, is the guard
 *     (DESIGN.md, Will it fit). A note takes what its register needs; when
 *     the pool or the voices run out, the quietest notes are stolen, and a
 *     stolen note fades over 3 ms as a ghost instead of clicking off.
 */
#include <math.h>
#include <string.h>

#include "quilt.h"

#define TWO_PI 6.28318530718f

int quilt_sway_is_tremolo(int inst) {
    /* On the vibraphone SWAY is the fan's depth (modal.c), not a tremolo. */
    return strcmp(QUILT_INST[inst].name, "Vibraphone") != 0;
}

int quilt_modal_in_use(const quilt_t *q) {
    int n = 0;
    for (int i = 0; i < QUILT_VOICES; i++)
        if (q->v[i].active) n += q->v[i].mv.n4 * 4;
    return n;
}

/* Moves a voice into a ghost slot to fade out, freeing its own slot. With
 * every ghost busy, the one furthest through its fade makes way. */
static void steal(quilt_t *q, int i) {
    voice_t *v = &q->v[i];
    if (!v->active) return;
    int slot = -1;
    float lowest = 2.0f;
    for (int g = QUILT_VOICES; g < QUILT_VOICES + QUILT_GHOSTS; g++) {
        if (!q->v[g].active) { slot = g; break; }
        if (q->v[g].fade < lowest) { lowest = q->v[g].fade; slot = g; }
    }
    q->v[slot] = *v;
    q->v[slot].held = 0;
    q->v[slot].fade_step = 1.0f / (0.003f * QUILT_SR);
    v->active = 0;
}

static int quietest(const quilt_t *q, int except_note) {
    int best = -1;
    float lo = 1e9f;
    for (int i = 0; i < QUILT_VOICES; i++) {
        const voice_t *v = &q->v[i];
        if (!v->active || v->note == except_note) continue;
        float l = v->mv.peak * (v->held ? 4.0f : 1.0f);   /* keep held notes longer */
        if (l < lo) { lo = l; best = i; }
    }
    return best;
}

void quilt_note_on(quilt_t *q, int note, int vel) {
    if (!modal_supports(q->type)) return;
    int slot = -1;
    for (int i = 0; i < QUILT_VOICES && slot < 0; i++)
        if (q->v[i].active && q->v[i].note == note) slot = i;
    if (slot >= 0) steal(q, slot);
    for (int i = 0; i < QUILT_VOICES && slot < 0; i++)
        if (!q->v[i].active) slot = i;
    if (slot < 0) {
        slot = quietest(q, -1);
        steal(q, slot);
    }

    int need = modal_osc_needed(q, q->type, note);
    while (quilt_modal_in_use(q) + need > MODAL_BUDGET) {
        int victim = quietest(q, note);
        if (victim < 0) break;
        steal(q, victim);
    }
    int granted = MODAL_BUDGET - quilt_modal_in_use(q);
    if (granted > need) granted = need;
    if (granted < 8) granted = 8;

    voice_t *v = &q->v[slot];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->held = 1;
    v->note = note;
    v->inst = q->type;
    v->vel = (float)vel / 127.0f;
    v->fade = 1.0f;
    modal_note_on(q, v, granted);
}

void quilt_note_off(quilt_t *q, int note) {
    for (int i = 0; i < QUILT_VOICES; i++) {
        voice_t *v = &q->v[i];
        if (v->active && v->note == note) v->held = 0;
    }
}

void quilt_pressure(quilt_t *q, int note, int value) {
    for (int i = 0; i < QUILT_VOICES; i++) {
        voice_t *v = &q->v[i];
        if (!v->active || (note >= 0 && v->note != note)) continue;
        v->press = (float)value / 127.0f;
        v->got_press = 1;
    }
}

void quilt_all_off(quilt_t *q) {
    for (int i = 0; i < QUILT_VOICES; i++)
        if (q->v[i].active) quilt_note_off(q, q->v[i].note);
    q->pedal = 0;
}

void quilt_render(quilt_t *q, float *left, float *right, int frames) {
    float board[QUILT_MAX_BLOCK];
    memset(left, 0, sizeof(float) * (size_t)frames);
    memset(right, 0, sizeof(float) * (size_t)frames);
    memset(board, 0, sizeof(float) * (size_t)frames);

    /* Knobs glide over about 20 ms instead of jumping, so turning one never
     * pops. What is read per sample ramps across the block from gs_prev. */
    memcpy(q->gs_prev, q->gs, sizeof(q->gs));
    const float k = 1.0f - expf(-(float)frames / (0.02f * QUILT_SR));
    for (int i = 0; i < G_COUNT; i++) q->gs[i] += (q->g[i] - q->gs[i]) * k;

    for (int i = 0; i < QUILT_VOICES + QUILT_GHOSTS; i++) {
        voice_t *v = &q->v[i];
        if (v->active) modal_render(q, v, left, right, board, frames);
    }

    /* One motor for every vibraphone note. */
    int vib = quilt_instrument_by_name("Vibraphone");
    float rate = q->charv[vib] > 0.02f ? 0.7f + 10.0f * q->charv[vib] : 0.0f;
    q->motor = fmodf(q->motor + TWO_PI * rate * (float)frames / QUILT_SR, TWO_PI);

    fx_process(q, left, right, board, frames);
}
