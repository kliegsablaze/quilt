/*
 * Quilt — shared types. See DESIGN.md.
 *
 * Every table here is `static const` data compiled in: since Schwung 1.7.0 one
 * instance can be built on the loader thread while another renders, so
 * nothing shared is ever written after load (DESIGN.md, Implementation notes).
 */
#ifndef QUILT_H
#define QUILT_H

#include <stdint.h>

#include "contact.h"

#define QUILT_SR 44100.0f
#define QUILT_VOICES 16
#define QUILT_GHOSTS 8          /* stolen voices fading out over a few ms */
#define QUILT_MAX_BLOCK 256
#define MODAL_MAXOSC 160        /* 72 string modes, two strings each, plus spare */
#define MODAL_BUDGET 1536       /* oscillators across all voices; see Will it fit */
#define QUILT_NINST 38
#define QUILT_NTYPES 15        /* the instruments TYPE offers: those built so far */
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

extern const instrument_t QUILT_INST[QUILT_NINST];
extern const shape_def_t QUILT_SHAPES[SH_COUNT];
extern const param_def_t QUILT_GLOBALS[G_COUNT];
extern const param_def_t QUILT_TYPE_PARAM;   /* options filled from QUILT_INST */
extern const char *const QUILT_TYPE_NAMES[QUILT_NTYPES];

int quilt_instrument_by_name(const char *name);       /* -1 if none */
int quilt_type_inst(int t);         /* TYPE option -> instrument */
int quilt_type_index(int inst);     /* instrument -> TYPE option, or -1 if not offered */
const char *quilt_voicing(int inst);   /* "key=value" pairs; see instruments.c */
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
    contact_t ct;
    float strike_args[6];   /* mass, K, p, alpha, then kind-specific */
    float x0, f0, v0;
    float level;            /* scale of everything this voice outputs */
    float pan_l, pan_r, body;
    float thump, thump_env, thump_lp, thump_k, thump_a;
    float hiss_env, hiss_lp, hiss_k, hiss_a;
    float buzz_z, buzz_z2;  /* the kalimba's buzzers */
    float held_t, roll_t, roll_vel;
    int roll_flip;
    uint32_t rng;
    float peak;             /* largest output in the last block */
} modal_voice_t;

typedef struct {
    int active, held, note, inst;
    float fade, fade_step; /* ghosts only */
    float vel;
    float press;          /* last pad pressure, 0..1 */
    int got_press;
    modal_voice_t mv;
} voice_t;

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
} fx_t;

typedef struct {
    int type, preset;
    float g[G_COUNT];               /* as set */
    float gs[G_COUNT], gs_prev[G_COUNT];   /* as heard: gliding toward g, this block and last */
    float charv[QUILT_NINST];
    float slot[QUILT_NINST][QUILT_MAX_SHAPE_KEYS];
    int pedal;            /* CC64 */
    float modwheel;       /* CC1 */
    float lfo, motor;
    voice_t v[QUILT_VOICES + QUILT_GHOSTS];
    fx_t fx;
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
int quilt_modal_in_use(const quilt_t *q);

/* modal.c */
int modal_supports(int inst);
void modal_note_on(quilt_t *q, voice_t *v, int granted_osc);
int modal_osc_needed(const quilt_t *q, int inst, int note);
void modal_render(quilt_t *q, voice_t *v, float *left, float *right, float *board, int frames);
float quilt_string_B(int inst, int note, float stiff);   /* for the physics tests */
float quilt_note_freq(int inst, int note);
float quilt_bar_ratio(int inst, int k);   /* a bar's declared mode ratio, or 0 */

/* fx.c */
void fx_reset(fx_t *fx);
void fx_process(quilt_t *q, float *left, float *right, float *board, int frames);

#endif
