/*
 * The recipes: thirty-eight instruments, the nine Instrument-page shapes and
 * the shared Main and Effects keys. DESIGN.md, The thirty-eight instruments
 * and Control surface.
 *
 * Labels follow DESIGN.md's rule: the cell is one plain word of five letters
 * or fewer, naming what you hear as the knob turns up; the header says it in
 * full. tests/plan.test.mjs checks every cell against the host's own fitter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quilt.h"

#define F(k, c, n, d) { k, c, n, PK_FLOAT, 0.0f, 1.0f, d, NULL, NULL, 0 }
#define I(k, c, n, lo, hi, d) { k, c, n, PK_INT, lo, hi, d, NULL, NULL, 0 }
#define E(k, c, n, opts, d) \
    { k, c, n, PK_ENUM, 0.0f, (float)(sizeof(opts) / sizeof(opts[0]) - 1), d, NULL, \
      opts, (int)(sizeof(opts) / sizeof(opts[0])) }
#define N(a) ((int)(sizeof(a) / sizeof(a[0])))

/* Where a held note's level comes from (DESIGN.md, Playing it with the Move). */
static const char *const PRESS_OPTS[] = { "Pad", "Auto", "Blend" };

static const param_def_t MODAL_KEYS[] = {
    F("m_split", "Split", "Unison Detune", 0.3f),
    F("m_stiff", "Stiff", "String Stiffness", 0.5f),
    F("m_spot", "Spot", "Strike Point", 0.3f),
    F("m_body", "Body", "Body", 0.5f),
    F("m_noise", "Noise", "Mechanism Noise", 0.4f),
    F("m_damp", "Damp", "Damper", 0.5f),
    F("m_pedal", "Pedal", "Sustain Pedal", 0.0f),
};

static const param_def_t PLUCKED_KEYS[] = {
    F("p_spot", "Spot", "Pluck Point", 0.3f),
    F("p_body", "Body", "Body", 0.5f),
    F("p_edge", "Edge", "Edge (Brightness)", 0.5f),
    F("p_noise", "Noise", "Finger Noise", 0.3f),
    F("p_damp", "Damp", "Damper", 0.3f),
    F("p_stiff", "Stiff", "String Stiffness", 0.3f),
};

static const param_def_t BOWED_KEYS[] = {
    F("b_body", "Body", "Body", 0.6f),
    F("b_edge", "Edge", "Edge (Brightness)", 0.4f),
    E("b_press", "Press", "Pressure Source", PRESS_OPTS, 0.0f),
    F("b_swell", "Swell", "Swell Time", 0.3f),
    F("b_noise", "Noise", "Bow Noise", 0.3f),
    F("b_bite", "Bite", "Bow Bite", 0.3f),
};

static const param_def_t BANDED_KEYS[] = {
    F("d_bow", "Bow", "Bow Speed", 0.4f),
    F("d_blur", "Blur", "Band Width", 0.2f),
    F("d_hit", "Hit", "Strike Mix", 0.0f),
    E("d_press", "Press", "Pressure Source", PRESS_OPTS, 0.0f),
    F("d_swell", "Swell", "Swell Time", 0.4f),
    F("d_noise", "Noise", "Friction Noise", 0.2f),
};

static const param_def_t AIR_KEYS[] = {
    F("a_onset", "Onset", "Tonguing", 0.3f),
    F("a_edge", "Edge", "Edge (Brightness)", 0.4f),
    E("a_press", "Press", "Pressure Source", PRESS_OPTS, 0.0f),
    F("a_swell", "Swell", "Swell Time", 0.2f),
    F("a_air", "Air", "Breath Noise", 0.4f),
};

static const param_def_t ORGAN_KEYS[] = {
    F("o_ping", "Ping", "Percussion", 0.0f),
    F("o_tail", "Tail", "Percussion Tail", 0.3f),
    F("o_click", "Click", "Key Click", 0.3f),
    F("o_leak", "Leak", "Wheel Leakage", 0.2f),
    F("o_slow", "Slow", "Leslie Slow Speed", 0.3f),
    F("o_fast", "Fast", "Leslie Fast Speed", 0.6f),
    I("o_1", "1'", "Drawbar 1'", 0, 8, 0),
};

/* Hammond registration 00 8800 000 (DESIGN.md, Banks), 1' on the page above. */
static const param_def_t ORGAN_DRAWBARS[] = {
    I("o_16", "16'", "Drawbar 16'", 0, 8, 0),
    I("o_513", "5.3'", "Drawbar 5 1/3'", 0, 8, 0),
    I("o_8", "8'", "Drawbar 8'", 0, 8, 8),
    I("o_4", "4'", "Drawbar 4'", 0, 8, 8),
    I("o_223", "2.7'", "Drawbar 2 2/3'", 0, 8, 0),
    I("o_2", "2'", "Drawbar 2'", 0, 8, 0),
    I("o_135", "1.6'", "Drawbar 1 3/5'", 0, 8, 0),
    I("o_113", "1.3'", "Drawbar 1 1/3'", 0, 8, 0),
};

static const param_def_t MACHINE_KEYS[] = {
    F("k_edge", "Edge", "Edge (Brightness)", 0.4f),
    F("k_swell", "Swell", "Swell Time", 0.4f),
    F("k_8", "8'", "8' Register", 0.8f),
    F("k_4", "4'", "4' Register", 0.4f),
    F("k_2", "2'", "2' Register", 0.0f),
};

static const param_def_t FM_KEYS[] = {
    F("f_tune", "Tune", "Bell Tuning", 0.5f),
    F("f_tine", "Tine", "Tine Depth", 0.4f),
    F("f_split", "Split", "Unison Detune", 0.2f),
};

static const param_def_t CHOIR_KEYS[] = {
    I("v_crowd", "Crowd", "Singers", 1, 3, 3),
    F("v_air", "Air", "Breath", 0.3f),
    F("v_swell", "Swell", "Swell Time", 0.4f),
    E("v_press", "Press", "Pressure Source", PRESS_OPTS, 0.0f),
    F("v_split", "Split", "Unison Detune", 0.3f),
};

const shape_def_t QUILT_SHAPES[SH_COUNT] = {
    [SH_MODAL] = { "m_", MODAL_KEYS, N(MODAL_KEYS), NULL, 0, NULL, NULL, 0 },
    [SH_PLUCKED] = { "p_", PLUCKED_KEYS, N(PLUCKED_KEYS), NULL, 0, NULL, NULL, 0 },
    [SH_BOWED] = { "b_", BOWED_KEYS, N(BOWED_KEYS), NULL, 0, NULL, NULL, 1 },
    [SH_BANDED] = { "d_", BANDED_KEYS, N(BANDED_KEYS), NULL, 0, NULL, NULL, 1 },
    [SH_AIR] = { "a_", AIR_KEYS, N(AIR_KEYS), NULL, 0, NULL, NULL, 1 },
    [SH_ORGAN] = { "o_", ORGAN_KEYS, N(ORGAN_KEYS), ORGAN_DRAWBARS, N(ORGAN_DRAWBARS),
                   "drawbars", "Drawbars", 1 },
    [SH_MACHINE] = { "k_", MACHINE_KEYS, N(MACHINE_KEYS), NULL, 0, NULL, NULL, 1 },
    [SH_FM] = { "f_", FM_KEYS, N(FM_KEYS), NULL, 0, NULL, NULL, 0 },
    [SH_CHOIR] = { "v_", CHOIR_KEYS, N(CHOIR_KEYS), NULL, 0, NULL, NULL, 1 },
};

const param_def_t QUILT_GLOBALS[G_COUNT] = {
    [G_SOFT] = F("soft", "Soft", "Softness", 0.6f),
    [G_DECAY] = F("decay", "Decay", "Decay / Release", 0.5f),
    [G_SWAY] = F("sway", "Sway", "Sway Depth", 0.0f),
    [G_SPACE] = F("space", "Space", "Reverb Amount", 0.25f),
    [G_VOLUME] = { "volume", "Vol", "Volume", PK_FLOAT, -60.0f, 6.0f, 0.0f, "dB", NULL, 0 },
    [G_SPEED] = F("speed", "Speed", "Sway Speed", 0.4f),
    [G_SIZE] = F("size", "Size", "Reverb Size", 0.5f),
    [G_DARK] = F("dark", "Dark", "Reverb Darkness", 0.4f),
    [G_DELAY] = F("delay", "Delay", "Reverb Pre-delay", 0.2f),
    [G_TONE] = F("tone", "Tone", "Master Tone", 0.5f),
    [G_DRIVE] = F("drive", "Drive", "Drive", 0.0f),
};

#define K FAM_KEYS
#define M FAM_MALLETS
#define S FAM_STRINGS
#define G FAM_GLASS
#define B FAM_BREATH

/* name, slug, family, engine, shape, CHAR cell, CHAR header,
 * then Main's soft, decay, sway, space for the factory preset. */
const instrument_t QUILT_INST[QUILT_NINST] = {
    { "Felt Upright", "felt_upright", K, ENG_MODAL, SH_MODAL, "Felt", "Felt Strip", .55f, .5f, 0, .18f },
    { "Una Corda Grand", "una_corda_grand", K, ENG_MODAL, SH_MODAL, "Hush", "Soft Pedal (Una Corda)", .5f, .65f, 0, .28f },
    { "Electric Grand", "electric_grand", K, ENG_MODAL, SH_MODAL, "Twang", "Bridge Pickups", .5f, .5f, .2f, .2f },
    { "Clavichord", "clavichord", K, ENG_WAVEGUIDE, SH_PLUCKED, "Bend", "Bebung (Pressure Bend)", .5f, .4f, 0, .15f },
    { "Tine Piano", "tine_piano", K, ENG_MODAL, SH_MODAL, "Bark", "Tine Bark (Voicing)", .6f, .55f, .3f, .2f },
    { "Reed Piano", "reed_piano", K, ENG_MODAL, SH_MODAL, "Bite", "Reed Bite (Pickup Gap)", .6f, .5f, .25f, .2f },
    { "Celesta", "celesta", K, ENG_MODAL, SH_MODAL, "Bell", "Bell Overtones", .55f, .5f, 0, .3f },
    { "Toy Piano", "toy_piano", K, ENG_MODAL, SH_MODAL, "Bell", "Bell Overtones", .45f, .5f, 0, .22f },
    { "Tonewheel Organ", "tonewheel_organ", K, ENG_BANKS, SH_ORGAN, "Lush", "Scanner Chorus", .5f, .3f, 0, .2f },
    { "Flute Organ", "flute_organ", K, ENG_BANKS, SH_MACHINE, "Puff", "Chiff", .6f, .3f, .1f, .35f },
    { "Harmonium", "harmonium", K, ENG_AIR, SH_AIR, "Beat", "Celeste Beating", .6f, .3f, 0, .2f },
    { "Glass E.Piano", "glass_e_piano", K, ENG_SYNTHETIC, SH_FM, "Glass", "Glass Bell", .6f, .55f, .2f, .3f },
    { "String Ensemble", "string_ensemble", K, ENG_BANKS, SH_MACHINE, "Lush", "Ensemble Chorus", .6f, .5f, 0, .35f },

    { "Vibraphone", "vibraphone", M, ENG_MODAL, SH_MODAL, "Motor", "Fan Motor Speed", .5f, .6f, .45f, .3f },
    { "Marimba", "marimba", M, ENG_MODAL, SH_MODAL, "Roll", "Mallet Roll", .5f, .58f, 0, .21f },
    { "Xylophone", "xylophone", M, ENG_MODAL, SH_MODAL, "Roll", "Mallet Roll", .6f, .55f, 0, .22f },
    { "Glockenspiel", "glockenspiel", M, ENG_MODAL, SH_MODAL, "Bell", "Bell Overtones", .7f, .5f, 0, .28f },
    { "Tubular Bells", "tubular_bells", M, ENG_MODAL, SH_MODAL, "Mute", "Hand Damping", .5f, .55f, 0, .32f },
    { "Handbells", "handbells", M, ENG_MODAL, SH_MODAL, "Minor", "Minor Third (Tierce)", .6f, .55f, 0, .32f },
    { "Handpan", "handpan", M, ENG_MODAL, SH_MODAL, "Ring", "Harmonic Ring", .65f, .55f, 0, .3f },
    { "Tongue Drum", "tongue_drum", M, ENG_MODAL, SH_MODAL, "Ring", "Harmonic Ring", .6f, .5f, 0, .25f },
    { "Kalimba / Music Box", "kalimba_music_box", M, ENG_MODAL, SH_MODAL, "Buzz", "Mbira Buzzers", .6f, .55f, 0, .25f },

    { "Harp", "harp", S, ENG_WAVEGUIDE, SH_PLUCKED, "Bloom", "Sympathetic Ring", .6f, .7f, 0, .35f },
    { "Nylon Guitar", "nylon_guitar", S, ENG_WAVEGUIDE, SH_PLUCKED, "Bloom", "Sympathetic Ring", .6f, .5f, 0, .2f },
    { "Hammered Dulcimer", "hammered_dulcimer", S, ENG_MODAL, SH_MODAL, "Bloom", "Sympathetic Ring", .6f, .55f, 0, .25f },
    { "Pizzicato", "pizzicato", S, ENG_WAVEGUIDE, SH_PLUCKED, "Deep", "Viola to Cello", .7f, .35f, 0, .3f },
    { "Solo Cello", "solo_cello", S, ENG_WAVEGUIDE, SH_BOWED, "Veil", "Bow Position (Sul Tasto)", .6f, .4f, .3f, .3f },
    { "Solo Violin", "solo_violin", S, ENG_WAVEGUIDE, SH_BOWED, "Veil", "Bow Position (Sul Tasto)", .6f, .4f, .3f, .3f },
    { "String Section", "string_section", S, ENG_WAVEGUIDE, SH_BOWED, "Width", "Player Spread", .6f, .5f, .2f, .4f },

    { "Bowed Vibes", "bowed_vibes", G, ENG_BANDED, SH_BANDED, "Grip", "Bow Grip", .6f, .6f, 0, .35f },
    { "Glass Harmonica", "glass_harmonica", G, ENG_BANDED, SH_BANDED, "Wet", "Wet Finger", .7f, .5f, 0, .35f },
    { "Singing Bowl", "singing_bowl", G, ENG_BANDED, SH_BANDED, "Beat", "Mode Beating", .6f, .8f, 0, .35f },

    { "Flute", "flute", B, ENG_AIR, SH_AIR, "Cover", "Lip Cover", .6f, .3f, .25f, .3f },
    { "Pan Flute", "pan_flute", B, ENG_AIR, SH_AIR, "Puff", "Chiff", .6f, .3f, .15f, .35f },
    { "Ocarina", "ocarina", B, ENG_AIR, SH_AIR, "Pure", "Purity", .6f, .3f, .15f, .3f },
    { "Recorder", "recorder", B, ENG_AIR, SH_AIR, "Puff", "Chiff", .6f, .3f, .1f, .3f },
    { "Clarinet", "clarinet", B, ENG_AIR, SH_AIR, "Reed", "Reed Stiffness", .6f, .35f, .2f, .3f },
    { "Choir", "choir", B, ENG_SYNTHETIC, SH_CHOIR, "Vowel", "Vowel (Oo, Oh, Ah)", .6f, .5f, .2f, .45f },
};

/* TYPE offers only the instruments whose engine is built, so nothing on the
 * Move is a stand-in (DESIGN.md, Build order). Each joins this list, in the
 * catalogue's order, when its sound is done; tests check the two agree. */
const char *const QUILT_TYPE_NAMES[QUILT_NTYPES] = {
    "Felt Upright", "Una Corda Grand", "Electric Grand", "Tine Piano", "Reed Piano",
    "Celesta", "Toy Piano", "Tonewheel Organ", "Flute Organ", "Glass E.Piano", "String Ensemble",
    "Vibraphone", "Marimba", "Xylophone", "Glockenspiel", "Tubular Bells", "Handbells",
    "Handpan", "Tongue Drum", "Kalimba / Music Box", "Hammered Dulcimer",
};

const param_def_t QUILT_TYPE_PARAM = {
    "type", "Type", "Instrument", PK_ENUM, 0.0f, QUILT_NTYPES - 1, 0.0f, NULL,
    QUILT_TYPE_NAMES, QUILT_NTYPES
};

int quilt_type_inst(int t) { return quilt_instrument_by_name(QUILT_TYPE_NAMES[t]); }

int quilt_type_index(int inst) {
    for (int t = 0; t < QUILT_NTYPES; t++)
        if (!strcmp(QUILT_TYPE_NAMES[t], QUILT_INST[inst].name)) return t;
    return -1;
}

/* Each instrument's default sound, the one turning TYPE onto it brings back:
 * "key=value" pairs over every page. `char` is its CHAR, `m_...` its own
 * page, and Main or Effects keys by name; Main also starts from the table
 * above, and any Effects key not named here from its shared default.
 * VOL is never touched (DESIGN.md, Every instrument starts beautiful). */
static const struct { const char *name, *values; } VOICINGS[] = {
    /* Close and intimate (Frahm): a thick strip, the action loud and near, a
     * little presence, a small warm room. */
    { "Felt Upright", "char=0.35 m_noise=0.6 m_split=0.3 m_body=0.6 m_damp=0.3 "
                      "size=0.35 dark=0.55 delay=0.08 tone=0.55 drive=0.1" },
    /* Further off, on two strings, a touch darker, in a large hall. */
    { "Una Corda Grand", "char=0.45 m_noise=0.21 m_spot=0.25 m_split=0.25 m_body=0.55 m_damp=0.3 "
                         "size=0.65 dark=0.45 delay=0.3 tone=0.45" },
    /* A slow motor, released bars left to ring a little, a shimmering plate. */
    { "Vibraphone", "char=0.22 m_split=0.05 m_body=0.7 m_noise=0.15 m_damp=0.15 "
                    "size=0.6 dark=0.35 delay=0.25" },
    /* A CP-70 in a ballad: pickups a third up, a slow auto-pan, a modest room. */
    { "Electric Grand", "char=0.35 m_noise=0.3 m_body=0.5 m_split=0.2 m_damp=0.3 "
                        "size=0.45 dark=0.5 delay=0.12 speed=0.25" },
    /* A suitcase tine piano: voiced a little toward bark, the stereo
     * vibrato slow, a small room. */
    { "Tine Piano", "char=0.4 m_split=0.1 m_body=0.6 m_noise=0.25 m_damp=0.3 "
                    "size=0.45 dark=0.5 delay=0.12 speed=0.3" },
    /* A reed piano turned down in a small room: its own tremolo, the
     * amplifier just warm. */
    { "Reed Piano", "char=0.45 m_split=0.08 m_body=0.5 m_noise=0.25 m_damp=0.3 "
                    "size=0.35 dark=0.55 delay=0.08 speed=0.45 drive=0.15" },
    /* A Whiter Shade of Pale: flutes 00 8800 000, the Leslie slow, the
     * scanner part way to C3, the preamp a little warm. */
    { "Tonewheel Organ", "char=0.6 o_click=0.35 o_leak=0.25 o_8=8 o_4=8 "
                         "size=0.4 dark=0.5 delay=0.1 drive=0.2" },
    /* A chamber organ in a stone church: a stopped 8' and an open 4', a
     * little chiff, a gentle tremulant. */
    { "Flute Organ", "char=0.45 k_edge=0.25 k_swell=0.35 k_8=0.8 k_4=0.5 k_2=0 "
                     "size=0.7 dark=0.45 delay=0.25 speed=0.55" },
    /* An FM electric piano with its chorus: a little tink, the bell pair
     * on the twelfth, the pairs a few cents apart. */
    { "Glass E.Piano", "char=0.35 f_tune=0.5 f_tine=0.45 f_split=0.25 "
                       "size=0.5 dark=0.4 delay=0.15 speed=0.3" },
    /* Oxygene: viola and violin registers, a slow swell, the ensemble deep. */
    { "String Ensemble", "char=0.7 k_edge=0.3 k_swell=0.45 k_8=0.8 k_4=0.6 k_2=0 "
                         "size=0.65 dark=0.45 delay=0.2 speed=0.3" },
    /* Sugar-plum: felt hammers, a little of the bell, a medium room. */
    { "Celesta", "char=0.4 m_split=0.05 m_body=0.6 m_noise=0.3 m_damp=0.3 "
                 "size=0.55 dark=0.4 delay=0.2" },
    /* Slightly out of tune and clangy, in a small room (Cage). */
    { "Toy Piano", "char=0.5 m_split=0.15 m_body=0.6 m_noise=0.4 m_damp=0 "
                   "size=0.3 dark=0.5 delay=0.05" },
    /* Yarn mallets on rosewood, nearly dry. */
    { "Xylophone", "char=0 m_split=0.05 m_body=0.7 m_noise=0.3 m_damp=0 "
                   "size=0.45 dark=0.5 delay=0.15" },
    /* Soft-wrapped mallets, long-ringing steel, a little darker. */
    { "Glockenspiel", "char=0.35 m_split=0.05 m_body=0.3 m_noise=0.15 m_damp=0 "
                      "size=0.6 dark=0.45 delay=0.25 tone=0.45" },
    /* A thumb piano with a touch of its buzzers; BUZZ at zero is a music box. */
    { "Kalimba / Music Box", "char=0.3 m_split=0.1 m_body=0.7 m_noise=0.3 m_damp=0 "
                             "size=0.4 dark=0.5 delay=0.1" },
    /* Chimes in a church: a light hand, the pedal half down, a big room. */
    { "Tubular Bells", "char=0.2 m_split=0.1 m_body=0.15 m_noise=0.25 m_damp=0.1 "
                       "size=0.7 dark=0.45 delay=0.3" },
    /* Handbells, the tierce half in, the bells beating gently. */
    { "Handbells", "char=0.5 m_split=0.15 m_body=0.3 m_noise=0.2 m_damp=0 "
                   "size=0.65 dark=0.4 delay=0.25" },
    /* Fingertips on the tuned fields, the shell's air under the low notes. */
    { "Handpan", "char=0.55 m_split=0.15 m_body=0.6 m_noise=0.3 m_damp=0 "
                 "size=0.6 dark=0.45 delay=0.2" },
    /* Rubber mallets, a wooden box, a small room. */
    { "Tongue Drum", "char=0.5 m_split=0.1 m_body=0.65 m_noise=0.3 m_damp=0 "
                     "size=0.45 dark=0.5 delay=0.12" },
    /* Courses a little apart, struck near the bridge, the undamped strings blooming. */
    { "Hammered Dulcimer", "char=0.55 m_split=0.35 m_spot=0.2 m_body=0.8 m_noise=0.3 m_damp=0 "
                           "size=0.5 dark=0.45 delay=0.15" },
    /* Rosewood and yarn, nearly dry. */
    { "Marimba", "char=0 m_split=0.05 m_body=0.75 m_damp=0 m_noise=0.32 "
                 "size=0.45 dark=0.5 delay=0.15 tone=0.52" },
};

const char *quilt_voicing(int inst) {
    for (int i = 0; i < N(VOICINGS); i++)
        if (!strcmp(VOICINGS[i].name, QUILT_INST[inst].name)) return VOICINGS[i].values;
    return "";
}

int quilt_instrument_by_name(const char *name) {
    for (int i = 0; i < QUILT_NINST; i++)
        if (!strcmp(name, QUILT_INST[i].name)) return i;
    return -1;
}

int quilt_instrument_by_slug(const char *slug, int len) {
    for (int i = 0; i < QUILT_NINST; i++)
        if ((int)strlen(QUILT_INST[i].slug) == len && !strncmp(slug, QUILT_INST[i].slug, (size_t)len))
            return i;
    return -1;
}

int quilt_global_index(const char *key) {
    for (int i = 0; i < G_COUNT; i++)
        if (!strcmp(key, QUILT_GLOBALS[i].key)) return i;
    return -1;
}

int quilt_shape_key_count(shape_t s) {
    return QUILT_SHAPES[s].nkeys + QUILT_SHAPES[s].nextra;
}

const param_def_t *quilt_shape_param(shape_t s, int idx) {
    const shape_def_t *d = &QUILT_SHAPES[s];
    return idx < d->nkeys ? &d->keys[idx] : &d->extra[idx - d->nkeys];
}

int quilt_shape_key_in(shape_t s, const char *key) {
    int n = quilt_shape_key_count(s);
    for (int i = 0; i < n; i++)
        if (!strcmp(key, quilt_shape_param(s, i)->key)) return i;
    return -1;
}

int quilt_shape_key(const char *key, shape_t *shape_out) {
    for (int s = 0; s < SH_COUNT; s++) {
        const char *pre = QUILT_SHAPES[s].prefix;
        if (strncmp(key, pre, strlen(pre))) continue;
        int i = quilt_shape_key_in((shape_t)s, key);
        if (i >= 0) { *shape_out = (shape_t)s; return i; }
    }
    return -1;
}

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

int quilt_parse_value(const param_def_t *d, const char *val, float *out) {
    if (d->kind == PK_ENUM)
        for (int i = 0; i < d->noptions; i++)
            if (!strcmp(val, d->options[i])) { *out = (float)i; return 1; }
    char *end;
    float f = strtof(val, &end);
    if (end == val) return 0;
    if (d->kind == PK_FLOAT) { *out = clampf(f, d->min, d->max); return 1; }
    /* Ints and enum indices: a float index rounds, as the host's knobs send. */
    *out = clampf((float)(int)(f + 0.5f), d->min, d->max);
    return 1;
}

int quilt_format_value(const param_def_t *d, float v, char *buf, int buf_len) {
    int n;
    if (d->kind == PK_ENUM) n = snprintf(buf, (size_t)buf_len, "%s", d->options[(int)v]);
    else if (d->kind == PK_INT) n = snprintf(buf, (size_t)buf_len, "%d", (int)v);
    else n = snprintf(buf, (size_t)buf_len, "%.4f", (double)v);
    return (n < 0 || n >= buf_len) ? -1 : n;
}
