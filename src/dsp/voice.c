/*
 * Voices: how a note is started and released, and how voices are shared out.
 *
 *   - Every instrument TYPE offers plays through its engine: modal.c,
 *     fm.c, waveguide.c, or banks.c, whose keys are its own and take no
 *     voice.
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

static int named(int inst, const char *name) { return !strcmp(QUILT_INST[inst].name, name); }

int quilt_sway_is_tremolo(int inst) {
    /* SWAY is the vibraphone fan's depth (modal.c), the organ's Leslie
     * (fx.c) and the vibrato of the string machine, the bowed strings and
     * the winds (banks.c, waveguide.c, air.c), not a tremolo. */
    return !named(inst, "Vibraphone") && !named(inst, "Tonewheel Organ") && !named(inst, "String Ensemble") &&
           !bowed_supports(inst) && !air_sway_is_vibrato(inst);
}

int quilt_sway_is_pan(int inst) {
    /* The reed piano's tremolo is in its amplifier, and the flute organ's
     * and the harmonium's is a tremulant, in the wind: one channel, no pan. */
    return !named(inst, "Reed Piano") && !named(inst, "Flute Organ") && !named(inst, "Harmonium");
}

int quilt_speed_shown(int inst) {
    /* SPEED is the tremolo's or the vibrato's rate; the fan has MOTOR and
     * the Leslie its own two speeds. */
    return !named(inst, "Vibraphone") && !named(inst, "Tonewheel Organ");
}

int quilt_built(int inst) {
    return modal_supports(inst) || fm_supports(inst) || waveguide_supports(inst) || bowed_supports(inst) ||
           air_supports(inst) || banded_supports(inst) || banks_supports(inst);
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
    if (banks_supports(q->type)) { banks_note_on(q, note, vel); return; }
    const int fm = fm_supports(q->type), wg = waveguide_supports(q->type), bow = bowed_supports(q->type);
    const int air = air_supports(q->type), band = banded_supports(q->type);
    if (!fm && !wg && !bow && !air && !band && !modal_supports(q->type)) return;
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

    /* Only modal notes draw on the oscillator budget. */
    const int modal = !fm && !wg && !bow && !air && !band;
    int need = modal ? modal_osc_needed(q, q->type, note) : 0;
    while (quilt_modal_in_use(q) + need > MODAL_BUDGET) {
        int victim = quietest(q, note);
        if (victim < 0) break;
        steal(q, victim);
    }
    int granted = MODAL_BUDGET - quilt_modal_in_use(q);
    if (granted > need) granted = need;
    if (granted < 8 && modal) granted = 8;

    voice_t *v = &q->v[slot];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->held = 1;
    v->note = note;
    v->inst = q->type;
    v->vel = (float)vel / 127.0f;
    v->fade = 1.0f;
    if (fm) fm_note_on(q, v);
    else if (wg) waveguide_note_on(q, v);
    else if (bow) bowed_note_on(q, v);
    else if (air) air_note_on(q, v);
    else if (band) banded_note_on(q, v);
    else modal_note_on(q, v, granted);
}

void quilt_note_off(quilt_t *q, int note) {
    for (int i = 0; i < QUILT_VOICES; i++) {
        voice_t *v = &q->v[i];
        if (v->active && v->note == note) v->held = 0;
    }
    banks_note_off(q, note);
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
    for (int k = 0; k < BANK_KEYS; k++) banks_note_off(q, k);
    q->pedal = 0;
}

void quilt_render(quilt_t *q, float *left, float *right, int frames) {
    float board[QUILT_MAX_BLOCK], bridge[QUILT_MAX_BLOCK], bl[QUILT_MAX_BLOCK], br[QUILT_MAX_BLOCK];
    memset(left, 0, sizeof(float) * (size_t)frames);
    memset(right, 0, sizeof(float) * (size_t)frames);
    memset(board, 0, sizeof(float) * (size_t)frames);
    memset(bridge, 0, sizeof(float) * (size_t)frames);
    memset(bl, 0, sizeof(float) * (size_t)frames);
    memset(br, 0, sizeof(float) * (size_t)frames);

    /* Knobs glide over about 20 ms instead of jumping, so turning one never
     * pops. What is read per sample ramps across the block from gs_prev. */
    memcpy(q->gs_prev, q->gs, sizeof(q->gs));
    const float k = 1.0f - expf(-(float)frames / (0.02f * QUILT_SR));
    for (int i = 0; i < G_COUNT; i++) q->gs[i] += (q->g[i] - q->gs[i]) * k;

    for (int i = 0; i < QUILT_VOICES + QUILT_GHOSTS; i++) {
        voice_t *v = &q->v[i];
        if (!v->active) continue;
        if (fm_supports(v->inst)) fm_render(q, v, left, right, frames);
        else if (waveguide_supports(v->inst)) waveguide_render(q, v, left, right, bridge, frames);
        else if (bowed_supports(v->inst)) bowed_render(q, v, bl, br, frames);
        else if (air_supports(v->inst)) air_render(q, v, left, right, frames);
        else if (banded_supports(v->inst)) banded_render(q, v, left, right, frames);
        else modal_render(q, v, left, right, board, frames);
    }
    waveguide_bank_render(q, bridge, left, right, frames);
    bowed_body(q, bl, br, left, right, frames);
    banks_render(q, left, right, frames);

    /* One motor for every vibraphone note. */
    int vib = quilt_instrument_by_name("Vibraphone");
    float rate = q->charv[vib] > 0.02f ? 0.7f + 10.0f * q->charv[vib] : 0.0f;
    q->motor = fmodf(q->motor + TWO_PI * rate * (float)frames / QUILT_SR, TWO_PI);

    fx_process(q, left, right, board, frames);
}
