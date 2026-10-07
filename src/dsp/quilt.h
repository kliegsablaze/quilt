/*
 * Quilt — shared types. See DESIGN.md.
 *
 * Every table here is `static const` data compiled in: since Schwung 1.7.0 one
 * instance can be built on the loader thread while another renders, so
 * nothing shared is ever written after load (DESIGN.md, Implementation notes).
 */
#ifndef QUILT_H
#define QUILT_H

#include <math.h>
#include <stdint.h>

#include "contact.h"

#define QUILT_SR 44100.0f
#define QUILT_VOICES 16
#define QUILT_GHOSTS 8          /* stolen voices fading out over a few ms */
#define QUILT_MAX_BLOCK 256
#define MODAL_MAXOSC 160        /* 72 string modes, two strings each, plus spare */
#define MODAL_BUDGET 1536       /* oscillators across all voices; see Will it fit */
#define QUILT_NINST 38
#define QUILT_NTYPES 38        /* the instruments TYPE offers: those built so far */
#define QUILT_MAX_SHAPE_KEYS 16

typedef enum { FAM_KEYS, FAM_MALLETS, FAM_STRINGS, FAM_GLASS, FAM_BREATH } family_t;

typedef enum {
    ENG_MODAL, ENG_WAVEGUIDE, ENG_BANDED, ENG_AIR, ENG_BANKS, ENG_SYNTHETIC
} engine_t;

/* The nine Instrument-page shapes (DESIGN.md, Instrument page). */
typedef enum {
    SH_MODAL, SH_PLUCKED, SH_BOWED, SH_BANDED, SH_AIR,
    SH_ORGAN, SH_MACHINE, SH_FM, SH_CHOIR, SH_COUNT
} shape_t;

typedef enum { PK_FLOAT, PK_INT, PK_ENUM } pkind_t;

typedef struct {
    const char *key;      /* e.g. "m_split" */
    const char *cell;     /* short_name: one plain word, five letters or fewer */
    const char *name;     /* the held-knob header */
    pkind_t kind;
    float min, max, def;
    const char *unit;     /* PK_FLOAT only; NULL means a 0..1 percentage */
    const char *const *options;
    int noptions;
} param_def_t;

typedef struct {
    const char *prefix;              /* "m_" */
    const param_def_t *keys;         /* the Instrument page, after the CHAR cell */
    int nkeys;
    const param_def_t *extra;        /* a second page: the organ's drawbars */
    int nextra;
    const char *extra_level, *extra_label;
    int sustained;                   /* held notes are driven, not struck */
} shape_def_t;

typedef struct {
    const char *name;     /* TYPE option, the gate value and the factory preset name */
    const char *slug;     /* c_<slug>, level i_<slug>, state "<slug>.<key>" */
    family_t family;
    engine_t engine;
    shape_t shape;
    const char *char_cell, *char_name;
    float soft, decay, sway, space;          /* factory preset values for Main */
} instrument_t;

/* Main and Effects: one value each, shared by every instrument. */
enum {
    G_SOFT, G_DECAY, G_SWAY, G_SPACE, G_VOLUME,
    G_SPEED, G_SIZE, G_DARK, G_DELAY, G_TONE, G_DRIVE, G_COUNT
};

/* ---- Modulation: four modulators on their own page (mod.c; DESIGN.md,
 * Modulation). A modulator's knobs, in the order its destinations name them. */
#define QUILT_MODS 4
enum { MK_VELOCITY, MK_MPE, MK_LFO, MK_ENV };
enum {
    MP_KIND, MP_SHAPE, MP_RATE, MP_RISE, MP_FALL, MP_AXIS, MP_LAG,
    MP_AIM1, MP_DEPTH1, MP_AIM2, MP_DEPTH2, MP_COUNT
};

/* A note's own modulation sources. */
typedef struct {
    float env[QUILT_MODS], mpe[QUILT_MODS];   /* ENV and MPE, as this note hears them */
    float slide, bend;                        /* MPE input: CC74 0..1, pitch bend -1..1 */
    int chan;
    float gd[G_COUNT];                        /* last block's move of Main and Effects, for the glide */
} voice_mod_t;

/* One context's modulation, and what it laid over the knobs (mod_apply). */
typedef struct {
    int n, ns;
    int aim[2 * QUILT_MODS];
    float off[2 * QUILT_MODS];
    float *ptr[2 * QUILT_MODS], val[2 * QUILT_MODS];
    float g[G_COUNT], gs[G_COUNT], gsp[G_COUNT];
} mod_ctx_t;

extern const instrument_t QUILT_INST[QUILT_NINST];
extern const shape_def_t QUILT_SHAPES[SH_COUNT];
extern const param_def_t QUILT_GLOBALS[G_COUNT];
extern const param_def_t QUILT_TYPE_PARAM;   /* options filled from QUILT_INST */
extern const char *const QUILT_TYPE_NAMES[QUILT_NTYPES];

int quilt_instrument_by_name(const char *name);       /* -1 if none */
int quilt_type_inst(int t);         /* TYPE option -> instrument */
int quilt_type_index(int inst);     /* instrument -> TYPE option, or -1 if not offered */
const char *quilt_page_name(int inst);   /* its Instrument page's title */
extern int quilt_test_no_dither;          /* tests only (quilt.c) */
const char *quilt_voicing(int inst);   /* "key=value" pairs; see instruments.c */
/* Factory presets, QUILT_PER_TYPE per instrument, the instruments in
 * alphabetical order (TYPE keeps its families): the first is the
 * instrument's default sound, the others change it (presets.c). */
#define QUILT_PER_TYPE 3
#define QUILT_NPRESETS (QUILT_NTYPES * QUILT_PER_TYPE)
int quilt_preset_type(int preset);      /* preset -> TYPE option */
int quilt_type_first_preset(int t);     /* TYPE option -> its first preset */
const char *quilt_preset_name(int preset);
const char *quilt_preset_changes(int preset);   /* "key=value" pairs over the voicing */
int quilt_instrument_by_slug(const char *slug, int len);
int quilt_global_index(const char *key);
int quilt_shape_key_count(shape_t s);
const param_def_t *quilt_shape_param(shape_t s, int idx);
/* The shape and index of an Instrument-page key, or -1. */
int quilt_shape_key(const char *key, shape_t *shape_out);
int quilt_shape_key_in(shape_t s, const char *key);
/* Parses a written value for a parameter: a name or an index for an enum. */
int quilt_parse_value(const param_def_t *d, const char *val, float *out);
int quilt_format_value(const param_def_t *d, float v, char *buf, int buf_len);

/* ---- the instance ---- */

/* Noise heard as part of a note, not beside it (DESIGN.md, Noise). White
 * noise through a two-pole band-pass at the noise's own place, then two
 * one-pole low-passes at half as high again, so above it the noise falls
 * away 24 dB an octave, as fast as the soft instruments' own tone does,
 * never a fizz over the top. The band's peak gain is about Q, so what it
 * keeps is about as loud as before. */
typedef struct { float f, q, k, s1, s2, lp, lp2; } tint_t;
static inline void tint_set(tint_t *t, float fc, float Q) {
    fc = fminf(fc, 6000.0f);            /* the state-variable filter's safe range */
    t->f = 2.0f * sinf(3.14159265359f * fc / QUILT_SR);
    t->q = 1.0f / Q;
    t->k = 1.0f - expf(-6.28318530718f * 1.5f * fc / QUILT_SR);
}
/* The noise knobs turn on a square: fine steps among the gentle amounts,
 * and at full as loud as the coloured noise needs to be plainly heard. */
static inline float noise_amount(float knob, float full) { return knob * knob * full; }
static inline float tint(tint_t *t, float x) {
    t->s2 += t->f * t->s1;
    t->s1 += t->f * (x - t->s2 - t->q * t->s1);
    t->lp += (t->s1 - t->lp) * t->k;
    t->lp2 += (t->lp - t->lp2) * t->k;
    return t->lp2;
}

/* Pad pressure taking over a held note's level from its own swell. Until
 * the pad's first reading arrives the note plays on its swell alone; the
 * readings start a moment after the press, and taking over at once, from a
 * smoothed pressure still at nothing, stepped the level: one click, under
 * a second in, brighter up the keys. So the pressure takes over across
 * 250 ms: shorter, the fall from the swell to a light first press still
 * made a high clarinet's reed spit (replayed from a session logged on the
 * Move: +4 dB at 150 ms, +1 dB at 250). Pad: the pad, with a quarter of
 * the swell under it, so a held key never falls silent; Blend: half each. */
#define PRESS_HAND_STEP (1.0f / (0.25f * QUILT_SR))
/* The pads send a reading about every 29 ms (logged on the Move), so the
 * pressure is smoothed over 25 ms: each step rounded off, still quick. 12 ms
 * let a high clarinet's reed hear the steps. */
#define PRESS_SMOOTH_S 0.025f
static inline float press_level(float env, float press_s, float hand, int src) {
    const float pp = src == 0 ? fmaxf(press_s, 0.25f * env) : 0.5f * (env + press_s);
    return env + (pp - env) * hand;
}

typedef float v4 __attribute__((vector_size(16)));

/* One modal voice: up to MODAL_MAXOSC Mathews-Smith phasors, four at a time.
 * DESIGN.md, Modal engine. */
typedef struct {
    int n4;                 /* groups of four oscillators in use */
    v4 yr[MODAL_MAXOSC / 4], yi[MODAL_MAXOSC / 4];   /* state */
    v4 c[MODAL_MAXOSC / 4], s[MODAL_MAXOSC / 4];     /* r cos w, r sin w */
    v4 cw[MODAL_MAXOSC / 4], sw[MODAL_MAXOSC / 4];   /* cos w, sin w */
    v4 rr[MODAL_MAXOSC / 4], rd[MODAL_MAXOSC / 4];   /* r ringing, r damped */
    v4 in[MODAL_MAXOSC / 4];                         /* force -> mode gain */
    v4 am_mask;             /* group 0: oscillators the vibraphone fan moves */
    int has_am, damped, dampable, roll;
    float damp_x;           /* how far the dampers are down, 0..1, gliding */
    contact_t ct;
    float strike_args[6];   /* mass, K, p, alpha, then kind-specific */
    float x0, f0, v0;
    float level;            /* scale of everything this voice outputs */
    float pan_l, pan_r, body;
    float thump, thump_env, thump_a;
    float hiss_env, hiss_a;
    tint_t thump_t, hiss_t; /* the knock's and the damper's colour */
    float buzz_z, buzz_z2;  /* the kalimba's buzzers */
    float pu_norm, pu_comp, pu_prev;   /* the tine or reed pickup */
    float hp_x1, hp_y1, hp_x2, hp_y2;  /* the board's low cut, strings */
    float held_t, roll_t, roll_vel;
    int roll_flip;
    uint32_t rng;
} modal_voice_t;

/* One Glass E.Piano voice: two FM pairs, phases in cycles. DESIGN.md, Synthetic. */
typedef struct {
    float ph[4], inc[4];    /* tine carrier, tine modulator, bell carrier, bell modulator */
    float amp, amp_k, amp_kd;           /* level, its fall per sample ringing and damped */
    float ia, ia_k, ia_floor;           /* the tine pair's index, falling to a floor */
    float ib, ib_k;                     /* the bell pair's index */
    float bell, bell_k;                 /* the bell pair's level */
    float att, att_k;                   /* SOFT's attack */
    int damped, dampable;
} fm_voice_t;

/* One plucked string: DESIGN.md, Waveguide. The line is inline, not a
 * pointer, so a voice copied into a ghost slot keeps its own. */
#define WG_LINE 2048            /* the longest period: C1, 1349 samples */
#define WG_HIST 1024            /* the pluck's comb: up to half a period */
typedef struct {
    float line[WG_LINE];
    int w;
    float f0, P, D, tau;    /* the period, the read, the filters' phase delay at f0 */
    float bend;             /* the clavichord's Bebung: x f0, gliding */
    float g, gd, a, lz;     /* the loss: gain ringing and damped, pole, state */
    float ac, ax1, ay1;     /* the stiffness allpass */
    int dampable;
    float damp_x, g_was;    /* how far the hand is on the string, 0..1, gliding; the loss last block ended on */
    float level, pan_l, pan_r, body;
    /* the pluck, while it lasts */
    int ex_n, ex_len, hw;
    float amp, lpk, lp1, lp2, ik, il, p1, p2;
    float res_c[4], res_r2[4], res_g[4], res_y1[4], res_y2[4];
    float hist[WG_HIST], comb_d, comb_norm;
    /* the finger's own sound */
    float nz_env, nz_td, nz_a;
    tint_t nz_t;
    uint32_t rng;
} wg_voice_t;

/* BLOOM's open strings, shared by every note. */
#define WG_BANK_STRINGS 12
#define WG_BANK_LINE 1024       /* down to C2 */
typedef struct {
    float line[WG_BANK_LINE];
    int w;
    float D, g, a, lz;
} wg_bank_string_t;

typedef struct {
    wg_bank_string_t s[WG_BANK_STRINGS];
    int n, inst, active, fading;
    float fade, decay;      /* DECAY when tuned */
} wg_bank_t;

/* One bowed string: a neck side and a bridge side, the bow between them.
 * DESIGN.md, Waveguide. The String Section bows three per note. */
#define BOW_NECK 1024           /* down to C2, the bow at the bridge */
#define BOW_BRIDGE 256          /* the bow up to a quarter of the way along */
#define BOW_PLAYERS 3
typedef struct {
    float neck[BOW_NECK], bridge[BOW_BRIDGE];
    int wn, wb;
    float lz, hz;           /* the bridge's loss, the bow hair's width */
    float dcx, dcy;         /* the bridge's DC blocker */
    float r1, r2;           /* the corner's rounding, as the bridge hears it */
    float detune, pan_l, pan_r, vib_ph, vib_rate, onset;
    float Pt;               /* the period at the end of the last block, with the vibrato */
} bow_string_t;

typedef struct {
    bow_string_t s[BOW_PLAYERS];
    int n;
    float f0, P, beta, tau;
    float a, g, g_play, g_rel;   /* the loss: pole, gain now, bowed and lifted */
    float a_rel, fb, t60, amax, cw, budget;   /* to round the corners with the force */
    float dc_r, dc_mix;     /* the DC blocker, faded in once the bow has lifted */
    float env, att_k, rel_k, lvl; /* the automatic swell */
    float press_s;          /* pad pressure, smoothed */
    float bite, t, vib_in, lift;
    int Db;                 /* the bridge side: whole samples, and the fraction */
    float Db_frac;   /* lift: the bow on the string, 1, to off, 0 */
    float hair_k, force_mul, level, veil_comp;
    float nz_lp;
    tint_t nz_t;
    uint32_t rng;
} bow_voice_t;

/* One blown voice: a bore and a jet, a cavity, or a free reed. DESIGN.md, Air. */
#define AIR_BORE 1024           /* the bore, a period and a half at B3 */
#define AIR_JET 512
typedef struct {
    float bore[AIR_BORE], jet[AIR_JET];
    int bw, jw;
    float f0, D, Dj;        /* the bore's and the jet's delays */
    float lp, lpa, dcx, dcy;
    float cav_c, cav_r2, cav_g, cav_y1, cav_y2;   /* the vessel, or the reed chamber */
    float ph[2], inc[2], swing, speak_k, rank2;   /* the free reed and its céleste */
    float env, att_k, rel_k, lvl, press_s, onset, chiff;
    float nz_lp, vib_ph, t, level, pan_l, pan_r, jlp, jk, js, jamp, offset, jet_mul, noise_out, flow_lp, soft_k, jet_th, comp, rlp2, surge, fc, over, soft_eff, edge, charv, yamp;
    tint_t nz_t, chiff_t;
    float Db_was, Dj_was, comp_was, y0_was, jg_was, soft_k_was;   /* what the last block ended on */
    uint32_t rng;
} air_voice_t;

/* One bowed or rubbed object: a band per strong mode. DESIGN.md, Banded. */
#define BANDED_MODES 8
#define BANDED_LINE 512         /* a mode's period, down to A2 */
typedef struct {
    float line[BANDED_LINE];
    int w, li;
    float ap_c, ap_x1, ap_y1;   /* the read's fractional allpass, fixed for the note */
    float b0, a1, a2, x1, x2, y1, y2;   /* the band-pass */
    float amp, g, g_play, g_rel, back;
} band_t;

typedef struct {
    band_t band[BANDED_MODES];
    int n;
    float f0, env, att_k, rel_k, lvl, press_s, lift, hit;
    float r1, r2, nz_lp, level, pan_l, pan_r, beat_hz, wah_ph, touch;
    tint_t nz_t;
    uint32_t rng;
} banded_voice_t;

#define CHOIR_SINGERS 3
#define CHOIR_FORMANTS 5
typedef struct {
    float ph[CHOIR_SINGERS], inc[CHOIR_SINGERS], vib_ph[CHOIR_SINGERS], vib_mul[CHOIR_SINGERS];
    float drift[CHOIR_SINGERS], drift_to[CHOIR_SINGERS], drift_t[CHOIR_SINGERS], drift_amt[CHOIR_SINGERS];
    float ff[CHOIR_FORMANTS], fb[CHOIR_FORMANTS];            /* the formants, gliding */
    float cf[2][CHOIR_FORMANTS][3], z[2][CHOIR_FORMANTS][2]; /* each ear's cascade */
    float shift[2];
    int n, N;
    float a, aN1, aN2, norm, soft_eff, mix, crowd_norm;
    float env, att_k, rel_k, lvl, press_s, t, nz_lp, level, pan_l, pan_r, f0;
    uint32_t rng;
} choir_voice_t;

typedef struct {
    int active, held, note, inst;
    float fade, fade_step; /* ghosts only */
    float vel;
    float press;          /* last pad pressure, 0..1 */
    int got_press;
    float hand;           /* how far pad pressure has taken over, 0..1 */
    float peak;           /* largest output in the last block, for stealing */
    int osc;              /* modal oscillators held, against MODAL_BUDGET */
    voice_mod_t mod;
    /* A voice plays one engine at a time, so they share the space: a new
     * note clears, and a stolen one copies, the largest of them, not all. */
    union {
        modal_voice_t mv;
        fm_voice_t fm;
        wg_voice_t wg;
        bow_voice_t bow;
        air_voice_t air;
        banded_voice_t band;
        choir_voice_t choir;
    };
} voice_t;

/* The always-running banks: tonewheels, pipes and the string machine.
 * Keys open gates on generators that are shared (DESIGN.md, Banks). */
#define BANK_KEYS 128
#define BANK_WHEELS 96          /* 91 wheels, padded to groups of four */
typedef struct {
    int on, held, inst;
    int free;               /* struck on an instrument modulation chose, so TYPE does not let it go */
    int sus;                /* sounding as held: down, or caught by the pedal */
    float t;                /* since the key went down, s */
    float rel;              /* the release, 1 while held */
    float att, att_up;      /* pipe speech or string swell: fundamental, upper partials */
    float contact[9];       /* organ: when each drawbar's contact closes, s */
    int perc;               /* organ: struck while the percussion was fresh */
    float chiff, chiff_up, bp[2];   /* flute: the chiff's burst, its rise, its resonator */
    uint32_t rng;
} bank_key_t;

typedef struct {
    bank_key_t key[BANK_KEYS];
    /* tonewheels: Mathews-Smith phasors with r = 1, renormalised each block */
    v4 wr[BANK_WHEELS / 4], wi[BANK_WHEELS / 4], wc[BANK_WHEELS / 4], ws[BANK_WHEELS / 4];
    float wheel[QUILT_MAX_BLOCK][BANK_WHEELS];
    float perc;             /* the percussion's envelope, shared by every key */
    int perc_fresh;         /* triggered this block, so a chord's every note gets it */
    float click, click_lp, click_bp;
    float scan[512], scan_lp, scan_ph;   /* the scanner's delay line */
    int scan_w;
    /* pipes: one phase per pitch, shared by every rank that sounds it */
    float phase[128];
    /* string machine: twelve top-octave oscillators, divided down */
    float top_ph[12];
    uint32_t top_count[12];
    float vib_ph;
    float lp_z[4], bp_z[2];  /* SOFT's low-pass, EDGE's presence */
    float ens[1024], ens_slow, ens_fast;
    int ens_w;
} banks_t;

/* Shared effects: DESIGN.md, Shared effects. */
#define PLATE_LEN 8192
typedef struct {
    float board_z[8][2];                 /* soundboard resonators */
    float pre[8192]; int pre_w;          /* pre-delay, up to 185 ms */
    float pre_s;                         /* the pre-delay, gliding, in samples */
    float bw;                            /* input bandwidth */
    float ap_in[4][1024]; int ap_in_w[4];
    float tank_ap1[2][2048]; float tank_d1[2][PLATE_LEN];
    float tank_ap2[2][4096]; float tank_d2[2][PLATE_LEN];
    int w;                               /* shared write index for the tank */
    float damp_z[2], fb[2], lfo, lfo_s;
    float tilt_z[2][2], hp_z[2][2];   /* the two TONE shelves, per channel */
    /* The Leslie: horn above 800 Hz, drum below, each a Doppler delay. */
    float les_xo[2], les_horn[256], les_drum[256], les_hlp[2];
    int les_w;
    float horn_ang, drum_ang, horn_hz, drum_hz;
} fx_t;

typedef struct {
    int type, preset;
    float trim, trim_g;   /* the preset's level in dB, no knob (presets.c); its gain as heard */
    uint32_t dither;      /* the output's dither noise */
    float dither_g;       /* and its depth, from how loud the output is */
    float g[G_COUNT];               /* as set */
    float gs[G_COUNT], gs_prev[G_COUNT];   /* as heard: gliding toward g, this block and last */
    float charv[QUILT_NINST];
    float slot[QUILT_NINST][QUILT_MAX_SHAPE_KEYS];
    int pedal;            /* CC64 */
    int pluck_dir;        /* the next pluck's direction: fingers alternate */
    float modwheel;       /* CC1 */
    float lfo, motor;
    voice_t v[QUILT_VOICES + QUILT_GHOSTS];
    banks_t banks;
    wg_bank_t symp;
    struct { int inst; float z[2][4][2], x[2][2], dcx[2], dcy[2]; } bowbody;   /* the bowed strings' shared body */
    fx_t fx;
    /* Modulation (mod.c). */
    int mod_sel;                         /* the modulator the page shows, 0..3 */
    float mod[QUILT_MODS][MP_COUNT];     /* as set */
    struct mod_run {
        float e[MP_COUNT];               /* as heard this block, after the other modulators */
        float lfo, out;                  /* the LFO; the source's value for the newest note */
        float env, mpe;                  /* the newest note's envelope and MPE */
        float r0, r1;                    /* the random shapes' last and next values */
        double ph;
        uint32_t rng;
    } run[QUILT_MODS];
    float lead_vel, lead_press, lead_slide, lead_bend;   /* the newest note's */
    int lead_note, lead_chan, midi_chan, held_keys, type_by_mod;
    unsigned char key_down[128];
    float ch_slide[16], ch_bend[16];
    float shared_gd[G_COUNT];
    float roll_d;                       /* the mallets' roll sped up (+) or slowed (-), while a note renders */
    float vol_d, vol_d_prev;            /* VOL's move this block and last, in dB */
    char *hierarchy, *chain_params;
    int hierarchy_len, chain_params_len;
} quilt_t;

void quilt_reset_instrument(quilt_t *q, int inst);
void quilt_apply_preset(quilt_t *q, int preset);

int quilt_build_contracts(quilt_t *q);              /* contract.c */
int quilt_write_state(const quilt_t *q, char *buf, int buf_len);   /* state.c */
void quilt_read_state(quilt_t *q, const char *json);

void quilt_note_on(quilt_t *q, int note, int vel);  /* voice.c */
void quilt_note_off(quilt_t *q, int note);
void quilt_pressure(quilt_t *q, int note, int value);   /* note < 0: every note */
void quilt_all_off(quilt_t *q);
void quilt_render(quilt_t *q, float *left, float *right, int frames);
int quilt_sway_is_tremolo(int inst);
int quilt_sway_is_pan(int inst);
int quilt_speed_shown(int inst);
int quilt_built(int inst);
int quilt_modal_in_use(const quilt_t *q);

/* mod.c */
extern const param_def_t QUILT_MOD_SELECT;            /* "mod" */
extern const param_def_t QUILT_MOD_GAPS[2];           /* "modN_gap1", "modN_gap2" */
extern const param_def_t QUILT_MOD_PARAMS[MP_COUNT];  /* "modN_<key>" */
int quilt_mod_key(const char *key, int *mod_out);     /* MP_ index, or -1 */
int quilt_aim_count(void);
const char *quilt_aim_name(int i);
int quilt_rate_sync(float rate);        /* the bar division, or -1 when free */
float quilt_rate_beats(int sync);
float quilt_rate_hz(float rate);
struct host_api_v1;
void quilt_mod_set_host(const struct host_api_v1 *h);
void quilt_mod_init(quilt_t *q);
void quilt_mod_reset(quilt_t *q);
void mod_block(quilt_t *q, int frames);
void mod_voice(quilt_t *q, voice_t *v, int frames, mod_ctx_t *c);
void mod_shared(quilt_t *q, mod_ctx_t *c);
int mod_note(quilt_t *q, int note, int vel, mod_ctx_t *c, voice_mod_t *vm);
void mod_apply(quilt_t *q, mod_ctx_t *c, int inst, float *gd);
void mod_restore(quilt_t *q, const mod_ctx_t *c);
void quilt_slide(quilt_t *q, int ch, float x);
void quilt_bend(quilt_t *q, int ch, float x);

/* modal.c */
int modal_supports(int inst);
void modal_note_on(quilt_t *q, voice_t *v, int granted_osc);
int modal_osc_needed(const quilt_t *q, int inst, int note);
void modal_render(quilt_t *q, voice_t *v, float *left, float *right, float *board, int frames);
float quilt_string_B(int inst, int note, float stiff);   /* for the physics tests */
float quilt_note_freq(int inst, int note);
float quilt_bar_ratio(int inst, int k);   /* a bar's declared mode ratio, or 0 */

/* fm.c */
int fm_supports(int inst);
void fm_note_on(quilt_t *q, voice_t *v);
void fm_render(quilt_t *q, voice_t *v, float *left, float *right, int frames);

/* waveguide.c */
int waveguide_supports(int inst);
float waveguide_freq(int inst, int note);
void waveguide_note_on(quilt_t *q, voice_t *v);
void waveguide_render(quilt_t *q, voice_t *v, float *left, float *right, float *bridge, int frames);
void waveguide_bank_render(quilt_t *q, const float *bridge, float *left, float *right, int frames);
void waveguide_bank_reset(wg_bank_t *b);
int bowed_supports(int inst);
float bowed_freq(int inst, int note);
void bowed_note_on(quilt_t *q, voice_t *v);
void bowed_render(quilt_t *q, voice_t *v, float *left, float *right, int frames);
void bowed_body(quilt_t *q, const float *bl, const float *br, float *left, float *right, int frames);

/* air.c */
int air_supports(int inst);
int air_sway_is_vibrato(int inst);
float air_freq(int inst, int note);
void air_note_on(quilt_t *q, voice_t *v);
void air_render(quilt_t *q, voice_t *v, float *left, float *right, int frames);

/* banded.c */
int banded_supports(int inst);
float banded_freq(int inst, int note);
void banded_note_on(quilt_t *q, voice_t *v);
void banded_render(quilt_t *q, voice_t *v, float *left, float *right, int frames);

/* choir.c */
int choir_supports(int inst);
float choir_freq(int inst, int note);
void choir_note_on(quilt_t *q, voice_t *v);
void choir_render(quilt_t *q, voice_t *v, float *left, float *right, int frames);

/* banks.c */
int banks_supports(int inst);
void banks_reset(banks_t *b);
void banks_note_on(quilt_t *q, int note, int vel);
void banks_note_off(quilt_t *q, int note);
void banks_render(quilt_t *q, float *left, float *right, int frames);
int banks_active(const quilt_t *q);

/* sin(2 pi x) for x in cycles, to about 4e-6: a 9th-order series on the
 * nearest quarter-cycle. */
static inline float fast_sin(float x) {
    x -= (float)(int)(x + (x >= 0.0f ? 0.5f : -0.5f));
    if (x > 0.25f) x = 0.5f - x;
    else if (x < -0.25f) x = -0.5f - x;
    float y = x * 6.28318530718f, y2 = y * y;
    return y * (1.0f + y2 * (-1.0f / 6.0f + y2 * (1.0f / 120.0f + y2 * (-1.0f / 5040.0f + y2 * (1.0f / 362880.0f)))));
}

/* fx.c */
void fx_reset(fx_t *fx);
void fx_process(quilt_t *q, float *left, float *right, float *board, int frames);

#endif
