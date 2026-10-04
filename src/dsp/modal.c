/*
 * The modal engine: struck strings and struck bars. DESIGN.md, Modal engine.
 *
 * A ringing object is a set of decaying sine tones (modes). Each is a
 * Mathews-Smith phasor, y <- r e^{jw} y + in F, four multiplies per mode per
 * sample, run four at a time. Its frequency and decay can change at any
 * moment without a click.
 *
 *   Strings (Bank, Zambon & Fontana)
 *     f_k = k f0 sqrt(1 + B k^2), the stiff-string stretch, tuned on a
 *     Railsback curve. Losses b1 + b3 w^2 (Chaigne & Askenfelt). Each mode is
 *     two slightly detuned strings, after Weinreich: a "prompt" part that
 *     gives its energy to the bridge quickly and an "aftersound" part that
 *     sings on, which is the piano's double decay. Struck at a fraction x0 of
 *     the length, so mode k is excited by sin(k pi x0); heard at the bridge.
 *   Bars, plates, rods and tines
 *     Mode ratios and relative decays from the tuning of each instrument:
 *     undercut bars (vibraphone and marimba 1 : 4 : 10, xylophone 1 : 3),
 *     free bars and plates (glockenspiel, celesta, 1 : 2.76 : 5.40), and
 *     rods and tines clamped at one end (toy piano, kalimba,
 *     1 : 6.27 : 17.55), plus a resonator (tube, box or frame) on the
 *     fundamental, which the vibraphone's fan opens and closes.
 *
 * Both are driven by contact.c's force, so velocity and SOFT shape the tone
 * through the physics of the felt, not through a filter.
 */
#include <math.h>
#include <string.h>

#include "contact.h"
#include "quilt.h"

#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define MAX_STRING_MODES 72

typedef enum { MK_STRING, MK_BAR } modal_kind_t;

/* What each instrument's CHAR does (DESIGN.md, The thirty-eight instruments). */
typedef enum {
    CH_FELT, CH_HUSH, CH_BLOOM, CH_TWANG,                    /* strings */
    CH_MOTOR, CH_ROLL, CH_BELL, CH_BUZZ, CH_MUTE, CH_MINOR, CH_RING   /* bars */
} char_kind_t;

typedef struct {
    const char *name;
    modal_kind_t kind;
    float gain;
    /* strings */
    float decay_mult;         /* x the loss: an upright's shorter sustain */
    float bass_B_mult;        /* an upright's short bass strings are stiffer */
    float B_mult;             /* x the stiffness everywhere: a dulcimer's thin wire */
    float mass_mult, K_mult;  /* the hammer against a piano's: a dulcimer's beaters */
    int railsback;            /* octaves stretched, as a piano is tuned */
    float prompt_mult;        /* x the prompt sound's fast fall: slower on a dulcimer's courses */
    float split_mult;         /* x SPLIT's detune: a dulcimer's courses are tuned less closely */
    float wa;                 /* the second string's share: small on a piano, half on a course */
    int damper_top;           /* highest note with a damper; 0, none (DAMP is the hand) */
    /* bars */
    const float *ratio, *amp, *t60r;
    /* Per mode: 1, CHAR moves it (a bell's tierce, a handpan's tuned
     * overtones); 2, BODY does (the low, long modes of an instrument with no
     * resonator: a tube's lowest, a bell's hum). */
    const unsigned char *tag;
    int nmodes;
    float cavity_hz;          /* handpan, tongue drum: the body's air (Helmholtz) mode */
    float t60_ref, f_ref, t60_slope;   /* fundamental T60 at f_ref, falling as (f/f_ref)^-slope */
    float mass, K, p, kb, r;           /* mallet and bar point */
    float soft_comp;                   /* level given back as SOFT softens: tubes, bells */
    int dampers;              /* vibraphone, celesta, chimes: dampers on the pedal */
    /* both */
    char_kind_t ch;
    float transpose;          /* semitones above the key, as the parts are written */
    float pan_spread;
    float thump_hz, thump_ms, thump_gain;
} modal_recipe_t;

/* Vibraphone: aluminium bars tuned 1 : 4 : ~10, with weak torsional modes
 * that give the metal its shimmer. Relative T60 per mode. */
static const float VIB_RATIO[] = { 1.0f, 3.99f, 9.85f, 2.71f, 6.38f, 16.4f };
static const float VIB_AMP[] = { 1.0f, 0.9f, 0.3f, 0.035f, 0.025f, 0.03f };
static const float VIB_T60[] = { 1.0f, 0.42f, 0.17f, 0.30f, 0.18f, 0.08f };

/* Marimba: rosewood, 1 : 4 : 10, losses that take the overtones away fast. */
static const float MAR_RATIO[] = { 1.0f, 3.98f, 9.92f, 2.32f, 17.1f };
static const float MAR_AMP[] = { 1.0f, 0.8f, 0.25f, 0.02f, 0.02f };
static const float MAR_T60[] = { 1.0f, 0.28f, 0.10f, 0.25f, 0.05f };

/* Xylophone: rosewood undercut so the second mode is a twelfth, 1 : 3. */
static const float XYL_RATIO[] = { 1.0f, 3.0f, 6.17f, 10.2f };
static const float XYL_AMP[] = { 1.0f, 1.0f, 0.35f, 0.1f };
static const float XYL_T60[] = { 1.0f, 0.3f, 0.12f, 0.06f };

/* Glockenspiel: steel bars, not undercut, so a free bar's 1 : 2.76 : 5.40 :
 * 8.93. They ring for seconds. */
static const float GLK_RATIO[] = { 1.0f, 2.756f, 5.404f, 8.933f };
static const float GLK_AMP[] = { 1.0f, 0.55f, 0.3f, 0.15f };
static const float GLK_T60[] = { 1.0f, 0.5f, 0.3f, 0.18f };

/* Celesta: steel plates over wooden boxes, a free plate's ratios, the
 * fundamental carried by the box, the upper modes faint and quick. */
static const float CEL_RATIO[] = { 1.0f, 2.756f, 5.404f, 8.933f };
static const float CEL_AMP[] = { 1.0f, 0.5f, 0.2f, 0.08f };
static const float CEL_T60[] = { 1.0f, 0.35f, 0.2f, 0.1f };

/* Toy piano: hammers on rods clamped at one end, a cantilever's
 * 1 : 6.27 : 17.55 : 34.4, which is its clang. */
static const float TOY_RATIO[] = { 1.0f, 6.267f, 17.55f, 34.39f };
static const float TOY_AMP[] = { 1.0f, 0.8f, 0.3f, 0.1f };
static const float TOY_T60[] = { 1.0f, 0.35f, 0.15f, 0.08f };

/* Kalimba: steel tines, cantilevers too, over a box, plucked by the thumb. */
static const float KAL_RATIO[] = { 1.0f, 6.267f, 17.55f };
static const float KAL_AMP[] = { 1.0f, 0.6f, 0.15f };
static const float KAL_T60[] = { 1.0f, 0.2f, 0.08f };

/* Tubular bells: a free tube's modes rise as (2n+1)^2, 9 : 25 : 49 : 81 :
 * 121 : 169. Modes 4, 5 and 6 stand nearly 2 : 3 : 4, so the ear hears a
 * strike note an octave below mode 4 that no mode sounds (Rossing). Ratios
 * are to that strike note, the played pitch; mode 4 comes first. */
static const float TUB_RATIO[] = { 2.0f, 2.988f, 4.173f, 1.21f, 0.617f, 5.556f, 0.222f };
static const float TUB_AMP[] = { 1.0f, 0.8f, 0.55f, 0.3f, 0.15f, 0.25f, 0.05f };
static const float TUB_T60[] = { 1.0f, 0.8f, 0.6f, 1.3f, 1.6f, 0.4f, 2.0f };
static const unsigned char TUB_TAG[] = { 0, 0, 0, 2, 2, 0, 2 };

/* Handbells: prime, nominal, hum, tierce (the minor third), quint, and two
 * above, 1 : 2 : 0.5 : 1.2 : 1.5 : 2.5 : 3, the hum ringing longest. MINOR
 * moves the tierce. */
static const float HBL_RATIO[] = { 1.0f, 2.0f, 0.5f, 1.189f, 1.5f, 2.51f, 3.0f };
static const float HBL_AMP[] = { 1.0f, 0.6f, 0.5f, 0.45f, 0.25f, 0.2f, 0.15f };
static const float HBL_T60[] = { 1.0f, 0.5f, 1.6f, 0.8f, 0.7f, 0.35f, 0.3f };
static const unsigned char HBL_TAG[] = { 0, 0, 2, 1, 0, 0, 0 };

/* Handpan: each note field hammered to its fundamental, octave and twelfth,
 * with weaker untuned modes between (Morrison & Rossing). RING moves the
 * tuned two. */
static const float HPN_RATIO[] = { 1.0f, 2.0f, 3.0f, 2.38f, 4.21f };
static const float HPN_AMP[] = { 1.0f, 0.5f, 0.35f, 0.08f, 0.1f };
static const float HPN_T60[] = { 1.0f, 0.6f, 0.45f, 0.3f, 0.25f };
static const unsigned char HPN_TAG[] = { 0, 1, 1, 0, 0 };

/* Tongue drum: a cut steel tongue is a cantilever, 1 : 6.27, but shaped so
 * a mode sits near the twelfth and another near the octave. RING moves them. */
static const float TNG_RATIO[] = { 1.0f, 3.0f, 6.27f, 2.0f };
static const float TNG_AMP[] = { 1.0f, 0.4f, 0.35f, 0.15f };
static const float TNG_T60[] = { 1.0f, 0.4f, 0.15f, 0.5f };
static const unsigned char TNG_TAG[] = { 0, 1, 0, 1 };

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))

static const modal_recipe_t RECIPES[] = {
    { .name = "Felt Upright", .kind = MK_STRING, .gain = 0.54f, .decay_mult = 1.15f,
      .bass_B_mult = 2.0f, .B_mult = 1, .mass_mult = 1, .K_mult = 1, .railsback = 1, .prompt_mult = 1, .split_mult = 1, .wa = 0.18f,
      .damper_top = 88, .ch = CH_FELT, .pan_spread = 0.5f,
      .thump_hz = 700, .thump_ms = 14, .thump_gain = 1.64f },
    { .name = "Una Corda Grand", .kind = MK_STRING, .gain = 0.36f, .decay_mult = 1.0f,
      .bass_B_mult = 1.0f, .B_mult = 1, .mass_mult = 1, .K_mult = 1, .railsback = 1, .prompt_mult = 1, .split_mult = 1, .wa = 0.18f,
      .damper_top = 88, .ch = CH_HUSH, .pan_spread = 0.6f,
      .thump_hz = 600, .thump_ms = 12, .thump_gain = 1.3f },
    { .name = "Vibraphone", .kind = MK_BAR, .gain = 0.45f,
      .ratio = VIB_RATIO, .amp = VIB_AMP, .t60r = VIB_T60, .nmodes = N(VIB_RATIO),
      .t60_ref = 7.0f, .f_ref = 349.0f, .t60_slope = 0.6f,
      .mass = 0.003f, .K = 2e9f, .p = 2.3f, .kb = 4e5f, .r = 40.0f,
      .dampers = 1, .ch = CH_MOTOR, .pan_spread = 0.5f,
      .thump_hz = 2500, .thump_ms = 3, .thump_gain = 2.5f },
    { .name = "Marimba", .kind = MK_BAR, .gain = 0.63f,
      .ratio = MAR_RATIO, .amp = MAR_AMP, .t60r = MAR_T60, .nmodes = N(MAR_RATIO),
      .t60_ref = 1.3f, .f_ref = 261.6f, .t60_slope = 0.7f,
      .mass = 0.004f, .K = 8e8f, .p = 2.3f, .kb = 3e5f, .r = 40.0f,
      .ch = CH_ROLL, .pan_spread = 0.6f,
      .thump_hz = 1800, .thump_ms = 4, .thump_gain = 3.66f },
    /* Short, stiff strings heard through piezo pickups at the bridge, no
     * soundboard (the Yamaha CP-70). */
    { .name = "Electric Grand", .kind = MK_STRING, .gain = 0.53f, .decay_mult = 1.1f,
      .bass_B_mult = 4.0f, .B_mult = 1.5f, .mass_mult = 1, .K_mult = 1.0f, .railsback = 1, .prompt_mult = 0.4f, .split_mult = 1, .wa = 0.18f,
      .damper_top = 88, .ch = CH_TWANG, .pan_spread = 0.5f,
      .thump_hz = 800, .thump_ms = 10, .thump_gain = 2.04f },
    /* Light beaters, felt on one face, on thin wire courses with no dampers. */
    { .name = "Hammered Dulcimer", .kind = MK_STRING, .gain = 0.56f, .decay_mult = 1.2f,
      .bass_B_mult = 1.0f, .B_mult = 0.7f, .mass_mult = 0.5f, .K_mult = 1.0f, .railsback = 0, .prompt_mult = 0.3f, .split_mult = 2.0f, .wa = 0.4f,
      .damper_top = 0, .ch = CH_BLOOM, .pan_spread = 0.6f,
      .thump_hz = 1500, .thump_ms = 4, .thump_gain = 2.04f },
    /* Felt hammers through a keyboard action, with dampers; an octave up. */
    { .name = "Celesta", .kind = MK_BAR, .gain = 0.48f,
      .ratio = CEL_RATIO, .amp = CEL_AMP, .t60r = CEL_T60, .nmodes = N(CEL_RATIO),
      .t60_ref = 3.0f, .f_ref = 523.3f, .t60_slope = 0.6f,
      .mass = 0.003f, .K = 3e10f, .p = 2.7f, .kb = 8e5f, .r = 40.0f,
      .dampers = 1, .ch = CH_BELL, .transpose = 12, .pan_spread = 0.5f,
      .thump_hz = 900, .thump_ms = 10, .thump_gain = 2.56f },
    /* Small hammers on rods, no dampers; an octave up. */
    { .name = "Toy Piano", .kind = MK_BAR, .gain = 1.39f,
      .ratio = TOY_RATIO, .amp = TOY_AMP, .t60r = TOY_T60, .nmodes = N(TOY_RATIO),
      .t60_ref = 1.6f, .f_ref = 523.3f, .t60_slope = 0.5f,
      .mass = 0.0015f, .K = 2e10f, .p = 2.3f, .kb = 6e5f, .r = 40.0f,
      .ch = CH_BELL, .transpose = 12, .pan_spread = 0.4f,
      .thump_hz = 1200, .thump_ms = 6, .thump_gain = 6.95f },
    /* Yarn or rubber mallets instead of hard plastic; an octave up. */
    { .name = "Xylophone", .kind = MK_BAR, .gain = 0.81f,
      .ratio = XYL_RATIO, .amp = XYL_AMP, .t60r = XYL_T60, .nmodes = N(XYL_RATIO),
      .t60_ref = 0.9f, .f_ref = 523.3f, .t60_slope = 0.7f,
      .mass = 0.003f, .K = 3e9f, .p = 2.3f, .kb = 4e5f, .r = 40.0f,
      .ch = CH_ROLL, .transpose = 12, .pan_spread = 0.6f,
      .thump_hz = 2000, .thump_ms = 3, .thump_gain = 4.86f },
    /* Brass mallets wrapped soft, no dampers; two octaves up. */
    { .name = "Glockenspiel", .kind = MK_BAR, .gain = 0.61f,
      .ratio = GLK_RATIO, .amp = GLK_AMP, .t60r = GLK_T60, .nmodes = N(GLK_RATIO),
      .t60_ref = 6.0f, .f_ref = 1046.5f, .t60_slope = 0.5f,
      .mass = 0.002f, .K = 6e9f, .p = 2.3f, .kb = 1e6f, .r = 40.0f,
      .ch = CH_BELL, .transpose = 24, .pan_spread = 0.4f,
      .thump_hz = 3000, .thump_ms = 2, .thump_gain = 2.8f },
    /* The thumb: heavier and softer than a mallet, so the pluck is round. */
    { .name = "Kalimba / Music Box", .kind = MK_BAR, .gain = 0.62f,
      .ratio = KAL_RATIO, .amp = KAL_AMP, .t60r = KAL_T60, .nmodes = N(KAL_RATIO),
      .t60_ref = 2.5f, .f_ref = 523.3f, .t60_slope = 0.5f,
      .mass = 0.003f, .K = 6e9f, .p = 2.3f, .kb = 5e5f, .r = 40.0f,
      .ch = CH_BUZZ, .pan_spread = 0.5f,
      .thump_hz = 400, .thump_ms = 8, .thump_gain = 4.7f },
    /* Rawhide hammers on brass tubes, with a damper pedal. */
    { .name = "Tubular Bells", .kind = MK_BAR, .gain = 0.227f,
      .ratio = TUB_RATIO, .amp = TUB_AMP, .t60r = TUB_T60, .tag = TUB_TAG, .nmodes = N(TUB_RATIO),
      .t60_ref = 8.0f, .f_ref = 261.6f, .t60_slope = 0.5f,
      .mass = 0.01f, .K = 2e10f, .p = 2.85f, .kb = 2e6f, .r = 40.0f,
      .soft_comp = 0.2f, .dampers = 1, .ch = CH_MUTE, .pan_spread = 0.4f,
      .thump_hz = 1500, .thump_ms = 3, .thump_gain = 5.0f },
    /* Bronze bells, a leather clapper; an octave up. */
    { .name = "Handbells", .kind = MK_BAR, .gain = 0.255f,
      .ratio = HBL_RATIO, .amp = HBL_AMP, .t60r = HBL_T60, .tag = HBL_TAG, .nmodes = N(HBL_RATIO),
      .t60_ref = 6.0f, .f_ref = 523.3f, .t60_slope = 0.5f,
      .mass = 0.005f, .K = 1.6e10f, .p = 2.85f, .kb = 1.5e6f, .r = 40.0f,
      .soft_comp = 0.15f, .ch = CH_MINOR, .transpose = 12, .pan_spread = 0.5f,
      .thump_hz = 2000, .thump_ms = 2, .thump_gain = 4.0f },
    /* Fingers on hammered steel, over the shell's air. */
    { .name = "Handpan", .kind = MK_BAR, .gain = 0.24f,
      .ratio = HPN_RATIO, .amp = HPN_AMP, .t60r = HPN_T60, .tag = HPN_TAG, .nmodes = N(HPN_RATIO),
      .cavity_hz = 90.0f, .t60_ref = 4.0f, .f_ref = 261.6f, .t60_slope = 0.4f,
      .mass = 0.008f, .K = 1.5e8f, .p = 2.3f, .kb = 4e5f, .r = 40.0f,
      .ch = CH_RING, .pan_spread = 0.5f,
      .thump_hz = 300, .thump_ms = 6, .thump_gain = 7.5f },
    /* Rubber mallets on steel tongues over a box. */
    { .name = "Tongue Drum", .kind = MK_BAR, .gain = 0.35f,
      .ratio = TNG_RATIO, .amp = TNG_AMP, .t60r = TNG_T60, .tag = TNG_TAG, .nmodes = N(TNG_RATIO),
      .cavity_hz = 140.0f, .t60_ref = 2.5f, .f_ref = 261.6f, .t60_slope = 0.5f,
      .mass = 0.006f, .K = 3.8e10f, .p = 2.9f, .kb = 5e5f, .r = 40.0f,
      .ch = CH_RING, .pan_spread = 0.5f,
      .thump_hz = 500, .thump_ms = 5, .thump_gain = 6.2f },
};

static const modal_recipe_t *recipe_for(int inst) {
    for (int i = 0; i < N(RECIPES); i++)
        if (!strcmp(RECIPES[i].name, QUILT_INST[inst].name)) return &RECIPES[i];
    return NULL;
}

int modal_supports(int inst) { return recipe_for(inst) != NULL; }

float quilt_bar_ratio(int inst, int k) {
    const modal_recipe_t *r = recipe_for(inst);
    return (r && r->kind == MK_BAR && k < r->nmodes) ? r->ratio[k] : 0.0f;
}

static float slot(const quilt_t *q, int inst, const char *key, float fallback) {
    int i = quilt_shape_key_in(QUILT_INST[inst].shape, key);
    return i < 0 ? fallback : q->slot[inst][i];
}

/* Chaigne & Askenfelt's hammers and strings at C2, C4 and C7, between them
 * interpolated by key (geometrically for the quantities that span decades). */
static float by_note(int note, float c2, float c4, float c7, int geometric) {
    float t, a, b;
    if (note <= 60) { t = (float)(note - 36) / 24.0f; a = c2; b = c4; }
    else { t = (float)(note - 60) / 36.0f; a = c4; b = c7; }
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return geometric ? a * powf(b / a, t) : a + (b - a) * t;
}

float quilt_string_B(int inst, int note, float stiff) {
    const modal_recipe_t *r = recipe_for(inst);
    float B = note < 40 ? 1.5e-4f : 1.5e-4f * powf(10.0f, (float)(note - 40) / 48.0f * 1.8f);
    if (r && note < 48) B *= r->bass_B_mult;
    if (r) B *= r->B_mult;
    return B * powf(8.0f, 2.0f * stiff - 1.0f);
}

float quilt_note_freq(int inst, int note) {
    const modal_recipe_t *r = recipe_for(inst);
    float semis = (float)(note - 69) + (r ? r->transpose : 0.0f);
    if (r && r->railsback) {
        /* Railsback: a piano's octaves are stretched to meet its sharp partials. */
        float oct = semis / 12.0f;
        semis += 1.9f * oct * fabsf(oct) / 100.0f;
    }
    return 440.0f * powf(2.0f, semis / 12.0f);
}

static int string_modes(float f0, float B) {
    int k = 0;
    while (k < MAX_STRING_MODES) {
        float fk = (float)(k + 1) * f0 * sqrtf(1.0f + B * (float)((k + 1) * (k + 1)));
        if (fk > 15000.0f || fk > 0.45f * QUILT_SR) break;
        k++;
    }
    return k;
}

int modal_osc_needed(const quilt_t *q, int inst, int note) {
    const modal_recipe_t *r = recipe_for(inst);
    if (!r) return 0;
    int n;
    if (r->kind == MK_STRING) {
        float stiff = slot(q, inst, "m_stiff", 0.5f);
        n = 2 * string_modes(quilt_note_freq(inst, note), quilt_string_B(inst, note, stiff));
    } else {
        n = 2 * r->nmodes + 2;      /* the resonator, and the cavity if any */
    }
    return (n + 3) & ~3;
}

static void set_osc(modal_voice_t *m, int i, float f, float sigma, float sigma_d, float in) {
    if (f >= 0.48f * QUILT_SR) in = 0.0f;
    float w = TWO_PI * f / QUILT_SR;
    m->cw[i / 4][i % 4] = cosf(w);
    m->sw[i / 4][i % 4] = sinf(w);
    m->rr[i / 4][i % 4] = expf(-sigma / QUILT_SR);
    m->rd[i / 4][i % 4] = expf(-sigma_d / QUILT_SR);
    m->in[i / 4][i % 4] = in;
}

static void apply_damping(modal_voice_t *m, int damped) {
    for (int g = 0; g < m->n4; g++) {
        v4 r = damped ? m->rd[g] : m->rr[g];
        m->c[g] = r * m->cw[g];
        m->s[g] = r * m->sw[g];
    }
    m->damped = damped;
}

static float soft_mult(const quilt_t *q) {
    /* SOFT: felt or yarn hardness, a decade and a half either way of the middle. */
    return powf(10.0f, (0.5f - q->g[G_SOFT]) * 3.0f);
}

/* How loud the mechanism is: NOISE alone. */
static float noise_level(const quilt_t *q, int inst) {
    return slot(q, inst, "m_noise", 0.4f) * 1.1f;
}

static void strike(quilt_t *q, voice_t *v, const modal_recipe_t *r, float v0) {
    modal_voice_t *m = &v->mv;
    float *a = m->strike_args;
    if (r->kind == MK_STRING)
        contact_strike_string(&m->ct, a[0], a[1], a[2], a[3], a[4], m->x0, m->f0, v0);
    else
        contact_strike_bar(&m->ct, a[0], a[1], a[2], a[3], a[4], r->r, v0);
    /* The knock of the action or the mallet, heard and felt through the body. */
    m->thump_env = 1.0f;
    m->thump_a = r->thump_gain * 0.133f * v0 * noise_level(q, v->inst);
}

static uint32_t xorshift(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

static float noise(uint32_t *s) { return (float)(int32_t)xorshift(s) * (1.0f / 2147483648.0f); }

void modal_note_on(quilt_t *q, voice_t *v, int granted) {
    const int inst = v->inst, note = v->note;
    const modal_recipe_t *r = recipe_for(inst);
    modal_voice_t *m = &v->mv;
    memset(m, 0, sizeof(*m));
    m->rng = 0x9E3779B9u ^ (uint32_t)(note * 7919 + (int)(v->vel * 1000.0f));
    m->f0 = quilt_note_freq(inst, note);
    m->level = r->gain;
    m->body = slot(q, inst, "m_body", 0.5f);

    const float decay = q->g[G_DECAY];
    const float split = slot(q, inst, "m_split", 0.3f);
    const float spot = slot(q, inst, "m_spot", 0.3f);
    const float damp = slot(q, inst, "m_damp", 0.5f);
    const float charv = q->charv[inst];
    const float f0 = m->f0;
    int n = 0;

    if (r->kind == MK_STRING) {
        const float B = quilt_string_B(inst, note, slot(q, inst, "m_stiff", 0.5f));
        const float dm = powf(2.0f, (0.5f - decay) * 4.0f) * r->decay_mult;
        const float b1 = 0.35f * powf(f0 / 261.6f, 0.45f);
        const float b3 = 6e-9f;
        const float cents = (0.15f + 2.5f * split) * r->split_mult;
        float wa = r->wa;       /* the aftersound's share: about -15 dB on a piano */
        /* The prompt sound falls four or five times faster in the bass, less in the treble. */
        const float prompt = 1.0f + (0.6f + 3.0f * fminf(1.0f, fmaxf(0.0f, (float)(96 - note) / 60.0f))) * r->prompt_mult;
        /* SPOT: from right by the end (bright, every partial) to the middle
         * of the string, where the even partials vanish and it goes hollow. */
        m->x0 = 0.02f + 0.48f * powf(spot, 1.5f);
        /* With no dampers, DAMP is the player's hand. */
        m->dampable = note <= r->damper_top || (r->damper_top == 0 && damp > 0.01f);

        float mass = by_note(note, 4.9e-3f, 2.97e-3f, 2.2e-3f, 1) * r->mass_mult;
        const float K0 = by_note(note, 4e8f, 4.5e9f, 2.3e11f, 1) * r->K_mult;
        float K = K0 * soft_mult(q);
        float p = by_note(note, 2.3f, 2.5f, 3.0f, 0);
        float z = by_note(note, 2.5f, 1.62f, 0.9f, 1);
        float after = 0.8f, twang = 0.0f;   /* the aftersound's loss, the pickups' tilt */
        float bloom_cents = 0.0f, bloom = 0.0f;
        if (r->ch == CH_FELT) {
            /* FELT: the moderator strip, a much softer, thicker layer in the way. */
            K *= powf(0.01f, charv);
            p += 0.5f * charv;
            mass *= 1.0f + 0.5f * charv;
        } else if (r->ch == CH_HUSH) {
            /* HUSH: the soft pedal moves the hammer onto two strings, then one,
             * on unworn felt. The unstruck string sings through the bridge, so
             * the aftersound grows as the prompt sound falls. */
            K *= powf(0.05f, charv);
            wa += 0.55f * charv;
        } else if (r->ch == CH_BLOOM) {
            /* BLOOM: the other courses, never damped, take up the partials
             * they share and ring on, a little out of tune with the struck
             * one. They are more strings sounding, so they add to the
             * aftersound rather than take from the strike. */
            bloom = 0.6f * charv;
            after *= 1.0f - 0.5f * charv;
            bloom_cents = 2.0f * charv;
        } else if (r->ch == CH_TWANG) {
            /* TWANG: the piezo pickups hear the bridge's force, which rises
             * with the partial's number; up, the overtones come forward. */
            twang = 1.1f * charv;
        }
        /* Softer felt keeps the hammer on so long that even the fundamental is
         * lost; give most of that back, so SOFT and FELT change the tone, not
         * just the level. */
        m->level *= powf(K / K0, -0.15f);
        float *a = m->strike_args;
        a[0] = mass; a[1] = K; a[2] = p; a[3] = 2e-5f; a[4] = z;

        /* 4 f0 at the bridge per newton; the hammer's own register scaling
         * evens out the bass, and above middle C the board projects the
         * treble better, so a melody up there is not lost. */
        const float reg = f0 / 261.6f;
        const float g0 = 4.0f * f0 / QUILT_SR * 0.04f * powf(reg, reg > 1.0f ? 0.35f : -0.1f);
        int modes = string_modes(f0, B);
        if (modes * 2 > granted) modes = granted / 2;
        for (int k = 1; k <= modes; k++) {
            float stretch = sqrtf(1.0f + B * (float)(k * k));
            float fk = (float)k * f0 * stretch;
            float wk = TWO_PI * fk;
            float sigma = (b1 + b3 * wk * wk) * dm;
            /* DAMP: from a light damper that lets the note sigh away (about
             * 2 s) to one that stops it dead (about 50 ms). */
            float sigma_d = sigma + 3.0f + 135.0f * damp * damp;
            float g = g0 * sinf((float)k * PI * m->x0) / stretch;
            if (twang > 0.0f) g *= powf((float)k, twang);
            float det = powf(2.0f, ((k & 1) ? cents + bloom_cents : -cents - bloom_cents) / 1200.0f);
            set_osc(m, n++, fk, sigma * prompt, sigma_d, g * (1.0f - wa));
            /* The sympathetic ring dies in about 20 s at most, as a dulcimer's
             * does, longer only as DECAY asks. */
            float sig_a = r->ch == CH_BLOOM ? fmaxf(sigma * after, 0.3f * dm) : sigma * after;
            set_osc(m, n++, fk * det, sig_a, sigma_d, g * (wa + bloom));
        }
        m->pan_l = cosf((0.5f + 0.5f * r->pan_spread * (float)(note - 64) / 40.0f) * PI * 0.5f);
    } else {
        const float dm = powf(2.0f, (decay - 0.5f) * 4.0f);
        const float t60_1 = r->t60_ref * powf(f0 / r->f_ref, -r->t60_slope) * dm;
        const float g0 = 0.0006f * powf(f0 / 349.0f, 0.3f);
        const float stiff = slot(q, inst, "m_stiff", 0.5f);
        m->dampable = r->dampers || damp > 0.01f;
        float *a = m->strike_args;
        /* Graduated mallets, as players use: harder yarn up the instrument.
         * Higher bars are also shorter and stiffer, so the mallet leaves them
         * sooner, and their fundamental is not lost to a long, soft pulse. */
        const float reg = f0 / r->f_ref;
        a[0] = r->mass;
        a[1] = r->K * soft_mult(q) * fminf(20.0f, fmaxf(0.2f, powf(reg, 1.2f)));
        /* As for the strings: a heavy, soft mallet loses some fundamental
         * too; where it loses much, give part of it back, so SOFT is heard as
         * tone more than level. */
        if (r->soft_comp > 0.0f) m->level *= powf(soft_mult(q), -r->soft_comp);
        a[2] = r->p;
        a[3] = 3e-5f;
        a[4] = r->kb * fminf(30.0f, fmaxf(0.1f, powf(reg, 1.5f)));

        /* The tube: resonance on the fundamental, first, so the fan can move it. */
        float sigma1 = 6.91f / t60_1;
        set_osc(m, n++, f0, sigma1 * 1.6f + 1.5f, sigma1 + 4.0f + 135.0f * damp * damp,
                g0 * 0.9f * m->body);
        /* The fan opens and closes the tube, which carries most of the
         * fundamental: it moves the tube and the bar's own fundamental pair. */
        if (r->ch == CH_MOTOR) { m->has_am = 1; m->am_mask = (v4){ 1.0f, 1.0f, 1.0f, 0.0f }; }
        /* The body's air: one Helmholtz mode, driven most by the notes near it. */
        if (r->cavity_hz > 0.0f) {
            float oct = log2f(f0 / r->cavity_hz);
            set_osc(m, n++, r->cavity_hz, 9.0f, 9.0f + 135.0f * damp * damp,
                    g0 * 1.5f * m->body / (1.0f + 4.0f * oct * oct));
        }

        for (int k = 0; k < r->nmodes && n + 2 <= granted; k++) {
            /* STIFF nudges the upper modes sharp or flat of their tuning. */
            float ratio = r->ratio[k] * (1.0f + 0.08f * (stiff - 0.5f) * log2f(r->ratio[k]));
            float fk = f0 * ratio;
            float sigma = 6.91f / (t60_1 * r->t60r[k]);
            /* MUTE: the player's hand on the tube, taking the upper modes first. */
            if (r->ch == CH_MUTE) sigma += charv * (1.0f + 3.0f * r->ratio[k] / r->ratio[0]);
            float sigma_d = sigma + 4.0f + 135.0f * damp * damp;
            /* SPOT: from the centre (all fundamental) toward the end, where
             * the overtones come up by 20 dB. */
            float w = r->amp[k] * (k == 0 ? 1.0f : powf(10.0f, (spot - 0.3f) * 1.5f));
            /* BELL: the upper modes, 8 dB down to 8 dB up. */
            if (r->ch == CH_BELL && k > 0) w *= powf(10.0f, (charv - 0.5f) * 0.8f);
            /* MINOR: the tierce, from none (a bright, major-ish bell) to strong. */
            if (r->ch == CH_MINOR && r->tag && r->tag[k] == 1) w *= 2.0f * charv;
            /* RING: the tuned octave and twelfth, 12 dB down to 12 dB up. */
            if (r->ch == CH_RING && r->tag && r->tag[k] == 1) w *= powf(10.0f, (charv - 0.5f) * 1.2f);
            /* BODY, where there is no resonator: the deep, long modes. */
            if (r->tag && r->tag[k] == 2) w *= 2.0f * m->body;
            set_osc(m, n++, fk, sigma, sigma_d, g0 * w * (1.0f - 0.4f * split));
            /* SPLIT: a second, near-degenerate mode, so the note beats slowly. */
            set_osc(m, n++, fk * (1.0f + 0.006f * split), sigma * 1.1f, sigma_d, g0 * w * 0.7f * split);
        }
        m->roll = r->ch == CH_ROLL;
        m->pan_l = cosf((0.5f + 0.5f * r->pan_spread * (float)(note - 66) / 30.0f) * PI * 0.5f);
    }
    m->pan_r = sqrtf(fmaxf(0.0f, 1.0f - m->pan_l * m->pan_l));
    m->n4 = (n + 3) / 4;
    for (int i = n; i < m->n4 * 4; i++) set_osc(m, i, 0.0f, 0.0f, 0.0f, 0.0f);
    apply_damping(m, 0);

    m->thump_k = 1.0f - expf(-TWO_PI * r->thump_hz / QUILT_SR);
    m->hiss_k = 1.0f - expf(-TWO_PI * 1500.0f / QUILT_SR);
    float vel = v->vel;
    /* A key or pad's velocity is a speed, so hammer and mallet speed follow it
     * in proportion: about 0.3 to 4.5 m/s for the hammers (Chaigne &
     * Askenfelt's range), and a little less for the mallets. */
    m->v0 = r->kind == MK_STRING ? 0.3f + 4.2f * vel : 0.3f + 3.2f * vel;
    m->roll_t = 0.0f;
    strike(q, v, r, m->v0);
}

void modal_render(quilt_t *q, voice_t *v, float *left, float *right, float *board, int frames) {
    const modal_recipe_t *r = recipe_for(v->inst);
    modal_voice_t *m = &v->mv;
    const float dt = (float)frames / QUILT_SR;

    /* Dampers: a released note is damped unless a pedal holds it. Pianos have
     * none at the very top; a marimba is damped only by DAMP (the hand). */
    int pedal = q->pedal || slot(q, v->inst, "m_pedal", 0.0f) >= 0.5f;
    int damped = m->dampable && !v->held && !pedal;
    if (damped != m->damped) {
        apply_damping(m, damped);
        if (damped && r->kind == MK_STRING) {
            /* The damper felt landing. */
            m->hiss_env = 1.0f;
            m->hiss_a = 0.06f * noise_level(q, v->inst);
        }
    }

    /* ROLL: hold a marimba note and the mallets roll; press to roll faster. */
    if (m->roll && v->held && q->charv[v->inst] > 0.02f) {
        m->held_t += dt;
        if (m->held_t > 0.15f && (m->roll_t -= dt) <= 0.0f) {
            float rate = v->got_press ? 5.0f + 12.0f * v->press : 8.0f;
            m->roll_flip ^= 1;
            m->roll_t = (m->roll_flip ? 0.94f : 1.06f) / rate;
            float vel = m->v0 * (0.3f + 0.5f * q->charv[v->inst]) *
                        (v->got_press ? 0.5f + 0.7f * v->press : 1.0f) * (m->roll_flip ? 1.0f : 0.85f);
            strike(q, v, r, vel);
        }
    }

    float x[QUILT_MAX_BLOCK];
    int excite = m->ct.active;
    if (excite) for (int n = 0; n < frames; n++) x[n] = contact_step(&m->ct);

    if (!excite) memset(x, 0, sizeof(float) * (size_t)frames);
    v4 acc[QUILT_MAX_BLOCK];
    memset(acc, 0, sizeof(v4) * (size_t)frames);
    int g = 0;
    if (m->has_am) {
        /* The vibraphone's fan: one motor, its speed CHAR, its depth SWAY. The
         * fan's sine is a rotating phasor, not a sinf() per sample. */
        const float depth0 = q->gs_prev[G_SWAY], depth1 = q->gs[G_SWAY];
        const float rate = q->charv[v->inst] > 0.02f ? 0.7f + 10.0f * q->charv[v->inst] : 0.0f;
        const float inc = TWO_PI * rate / QUILT_SR, cr = cosf(inc), sr = sinf(inc);
        float pc = cosf(q->motor), ps = sinf(q->motor);
        const v4 one = { 1.0f, 1.0f, 1.0f, 1.0f };
        v4 yr = m->yr[0], yi = m->yi[0];
        const v4 c = m->c[0], s = m->s[0], in = m->in[0];
        for (int n = 0; n < frames; n++) {
            float depth = depth0 + (depth1 - depth0) * (float)(n + 1) / (float)frames;
            float am = rate > 0.0f ? 1.0f - depth * (0.5f + 0.5f * ps) : 1.0f;
            float t2 = pc * cr - ps * sr;
            ps = ps * cr + pc * sr;
            pc = t2;
            v4 xv = { x[n], x[n], x[n], x[n] };
            v4 t = c * yr - s * yi + in * xv;
            yi = s * yr + c * yi;
            yr = t;
            acc[n] += yi * (one - m->am_mask + m->am_mask * am);
        }
        m->yr[0] = yr;
        m->yi[0] = yi;
        g = 1;
    }
    /* Two groups (eight modes) per pass: half the traffic through acc[]. */
    for (; g + 1 < m->n4; g += 2) {
        v4 yra = m->yr[g], yia = m->yi[g], yrb = m->yr[g + 1], yib = m->yi[g + 1];
        const v4 ca = m->c[g], sa = m->s[g], ina = m->in[g];
        const v4 cb = m->c[g + 1], sb = m->s[g + 1], inb = m->in[g + 1];
        if (excite) {
            for (int n = 0; n < frames; n++) {
                v4 xv = { x[n], x[n], x[n], x[n] };
                v4 ta = ca * yra - sa * yia + ina * xv;
                v4 tb = cb * yrb - sb * yib + inb * xv;
                yia = sa * yra + ca * yia;
                yib = sb * yrb + cb * yib;
                yra = ta;
                yrb = tb;
                acc[n] += yia + yib;
            }
        } else {
            for (int n = 0; n < frames; n++) {
                v4 ta = ca * yra - sa * yia;
                v4 tb = cb * yrb - sb * yib;
                yia = sa * yra + ca * yia;
                yib = sb * yrb + cb * yib;
                yra = ta;
                yrb = tb;
                acc[n] += yia + yib;
            }
        }
        m->yr[g] = yra;
        m->yi[g] = yia;
        m->yr[g + 1] = yrb;
        m->yi[g + 1] = yib;
    }
    for (; g < m->n4; g++) {
        v4 yr = m->yr[g], yi = m->yi[g];
        const v4 c = m->c[g], s = m->s[g], in = m->in[g];
        for (int n = 0; n < frames; n++) {
            v4 xv = { x[n], x[n], x[n], x[n] };
            v4 t = c * yr - s * yi + in * xv;
            yi = s * yr + c * yi;
            yr = t;
            acc[n] += yi;
        }
        m->yr[g] = yr;
        m->yi[g] = yi;
    }

    const float td = expf(-1.0f / (r->thump_ms * 0.001f * QUILT_SR));
    const float buzz_a = r->ch == CH_BUZZ && q->charv[v->inst] > 0.02f ? 3.0f * q->charv[v->inst] : 0.0f;
    const float hd = expf(-1.0f / (0.03f * QUILT_SR));
    float peak = 0.0f;
    for (int n = 0; n < frames; n++) {
        float y = (acc[n][0] + acc[n][1] + acc[n][2] + acc[n][3]) * m->level;
        float knock = 0.0f;
        if (m->thump_env > 1e-4f) {
            m->thump_lp += (noise(&m->rng) - m->thump_lp) * m->thump_k;
            knock += m->thump_lp * m->thump_env * m->thump_a;
            m->thump_env *= td;
        }
        if (m->hiss_env > 1e-4f) {
            m->hiss_lp += (noise(&m->rng) - m->hiss_lp) * m->hiss_k;
            knock += m->hiss_lp * m->hiss_env * m->hiss_a;
            m->hiss_env *= hd;
        }
        if (buzz_a > 0.0f) {
            /* BUZZ: the buzzers chatter against the box whenever the tine
             * swings hard, so a band of noise rides the note's own swing. */
            float nz = noise(&m->rng);
            m->buzz_z += (nz - m->buzz_z) * 0.435f;         /* a sizzle, about 4 to 10 kHz */
            m->buzz_z2 += ((nz - m->buzz_z) - m->buzz_z2) * 0.76f;
            knock += m->buzz_z2 * fabsf(y) * buzz_a;
        }
        y *= v->fade;
        knock *= v->fade;
        if (v->fade_step > 0.0f) v->fade = fmaxf(0.0f, v->fade - v->fade_step);
        float a = fabsf(y);
        if (a > peak) peak = a;
        left[n] += y * m->pan_l + knock * 0.5f;
        right[n] += y * m->pan_r + knock * 0.5f;
        if (r->kind == MK_STRING) board[n] += (y + knock * 2.0f) * m->body;
    }
    m->peak = peak;

    /* Retire it once it has fallen 100 dB below full scale and nothing drives it. */
    if ((peak < 1e-5f && !m->ct.active && m->thump_env < 1e-4f && m->hiss_env < 1e-4f &&
         (!v->held || !m->roll)) || (v->fade_step > 0.0f && v->fade <= 0.0f))
        v->active = 0;
}
