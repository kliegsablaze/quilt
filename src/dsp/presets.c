/*
 * The factory presets: three per instrument, in TYPE's order (DESIGN.md,
 * Picking an instrument). The first is the instrument's default sound,
 * named after it, and is what turning TYPE loads. The other two each name
 * a familiar way of playing it and change only what that needs, as
 * "key=value" pairs over the voicing in instruments.c. `trim` is the
 * preset's own level in dB, which has no knob: it brings each variation to
 * the defaults' loudness, -26.5 LUFS on the test phrase, as VOL would.
 *
 * Names fit the preset page: twenty characters of plain ASCII, the
 * instrument's own word first so the screen says what is playing.
 * tests/test_quilt.c checks the names, the keys, the loudness and that
 * every preset stays inside full scale.
 */
#include <stdlib.h>
#include <string.h>

#include "quilt.h"

typedef struct { const char *name, *changes; } variation_t;

static const struct {
    const char *inst;
    variation_t v[QUILT_PER_TYPE - 1];
} PRESETS[] = {
    /* Keys */
    { "Felt Upright", {
        /* The strip lifted out: bare hammers, played out, dry and close. */
        { "Upright Bare Hammer", "char=0 soft=0.3 tone=0.62 m_stiff=0.5 m_noise=0.3 m_damp=0.55 decay=0.4 trim=6.1" },
        /* A thicker strip, the pedal half down, a darker room. */
        { "Felt Lullaby", "char=0.6 soft=0.65 tone=0.48 m_pedal=0.35 decay=0.6 space=0.26 size=0.5 dark=0.6 trim=1.2" } } },
    { "Una Corda Grand", {
        /* The soft pedal up: all three strings, the lid open, near and bright. */
        { "Grand Open Lid", "char=0 soft=0.38 tone=0.58 m_spot=0.1 m_stiff=0.5 m_noise=0.35 size=0.4 trim=8.7" },
        /* Further back in the hall, the sustain pedal half down. */
        { "Grand Far Hall", "char=0.6 m_pedal=0.45 space=0.42 size=0.8 delay=0.4 dark=0.55 trim=-0.6" } } },
    { "Electric Grand", {
        /* Played out, pickups near the bridge, no pan, the amp pushed, dry. */
        { "E.Grand Bright", "char=0.75 soft=0.35 tone=0.62 sway=0 m_damp=0.5 drive=0.3 decay=0.4 trim=-1.0" },
        /* Softly, the pedal half down, a slow wide auto-pan, more room. */
        { "E.Grand Drift", "char=0.15 soft=0.7 tone=0.45 sway=0.6 speed=0.15 m_pedal=0.4 space=0.3 size=0.6 trim=2.1" } } },
    { "Clavichord", {
        /* The lute stop: cloth on the strings, short and plucked. */
        { "Clavichord Lute", "p_damp=0.85 p_edge=0.4 char=0.3 decay=0.3 trim=2.2" },
        /* Bebung deep, so pressing a held key sings and wavers. */
        { "Clavichord Bebung", "char=0.9 p_damp=0.35 decay=0.5 space=0.2 trim=-1.4" } } },
    { "Tine Piano", {
        /* Tines voiced away from the pickups, a wide slow vibrato, the pedal down. */
        { "Tine Piano Bell", "char=0.05 soft=0.75 tone=0.45 m_split=0.2 sway=0.5 speed=0.25 m_pedal=0.3 space=0.25 trim=-0.3" },
        /* Voiced close, played hard, the amplifier pushed, no vibrato, dry. */
        { "Tine Piano Bark", "char=0.9 soft=0.3 tone=0.6 drive=0.45 sway=0 m_damp=0.5 decay=0.4 trim=-0.7" } } },
    { "Reed Piano", {
        /* A wide pickup gap, no tremolo, the pedal half down, a little room. */
        { "Reed Piano Sweet", "char=0.15 soft=0.75 tone=0.45 sway=0 drive=0 m_pedal=0.35 space=0.22 size=0.5 trim=-0.7" },
        /* The tremolo deep and quick, the reeds biting, the amp warm. */
        { "Reed Piano Tremolo", "char=0.7 soft=0.4 tone=0.58 sway=0.8 speed=0.6 drive=0.35 m_damp=0.5 trim=2.4" } } },
    { "Celesta", {
        /* Harder hammers, more of the bell, short and dry. */
        { "Celesta Bright", "char=0.8 soft=0.35 tone=0.62 m_damp=0.55 decay=0.4 trim=2.9" },
        /* Soft, the pedal half down, a large room. */
        { "Celesta Dream", "char=0.3 soft=0.7 m_pedal=0.5 space=0.45 size=0.75 delay=0.3 trim=-1.1" } } },
    { "Toy Piano", {
        /* Rods well out of tune with each other, struck hard, very clangy. */
        { "Toy Piano Broken", "m_split=0.6 char=0.9 soft=0.3 m_spot=0.4 tone=0.6 drive=0.2 trim=-1.8" },
        /* Played gently in a bedroom. */
        { "Toy Piano Lullaby", "soft=0.65 char=0.3 space=0.32 size=0.45 trim=-0.5" } } },
    { "Tonewheel Organ", {
        /* 88 8000 000: the gospel ballad, Leslie slow. */
        { "Organ 888 Ballad", "o_16=8 o_513=8 o_8=8 o_4=0 char=0.5 sway=0.1 trim=-2.4" },
        /* 00 8000 000 with third percussion, scanner off: jazz. */
        { "Organ Jazz Perc", "o_8=8 o_4=0 o_ping=0.7 o_tail=0.3 o_click=0.45 char=0 sway=0 trim=1.2" } } },
    { "Flute Organ", {
        /* The stopped 8' alone, no tremulant, in a bigger chapel. */
        { "Flute Organ Gedackt", "k_8=1 k_4=0 k_2=0 char=0.2 k_edge=0.05 tone=0.4 sway=0 space=0.25 size=0.8 trim=-3.7" } ,
        /* 8', 4' and 2' drawn, more chiff, no tremulant: the small plenum. */
        { "Flute Organ Full", "k_8=0.7 k_4=0.7 k_2=0.6 char=0.6 k_edge=0.5 tone=0.58 sway=0 space=0.12 trim=0.1" } } },
    { "Harmonium", {
        /* The celeste rank fully in, the reeds close and quick to speak. */
        { "Harmonium Celeste", "char=0.9 a_swell=0.1 a_edge=0.6 tone=0.58 trim=2.5" },
        /* Further off, breathier, swelling slowly. */
        { "Harmonium Distant", "char=0.25 a_air=0.32 a_swell=0.5 space=0.38 size=0.7 trim=-1.9" } } },
    { "Glass E.Piano", {
        /* More of the bell pair, played harder, no chorus, ringing longer. */
        { "Glass EP Bell", "char=0.75 f_tine=0.8 soft=0.4 tone=0.6 sway=0 decay=0.65 trim=0.6" },
        /* Soft, little tine, the chorus wide and slow, more room. */
        { "Glass EP Soft", "char=0.1 soft=0.8 f_tine=0.2 tone=0.42 sway=0.6 speed=0.2 space=0.25 size=0.6 trim=1.3" } } },
    { "String Ensemble", {
        /* The 8' alone, darker and slower: cellos and violas. */
        { "Ensemble Low", "k_8=1 k_4=0 k_2=0 k_edge=0.1 k_swell=0.7 tone=0.42 char=0.5 trim=0.7" },
        /* The 4' and 2' over the 8', quicker and brighter: the violins on top. */
        { "Ensemble High", "k_8=0.3 k_4=0.9 k_2=0.6 k_edge=0.55 k_swell=0.25 tone=0.58 char=0.9 trim=-1.4" } } },

    /* Mallets */
    { "Vibraphone", {
        /* The fan stopped, harder mallets, the bars damped: dry jazz vibes. */
        { "Vibes Motor Off", "sway=0 soft=0.35 tone=0.55 m_damp=0.5 decay=0.45 trim=1.7" },
        /* The fan fast and deep, soft mallets, the pedal down: the old ballroom. */
        { "Vibes Fast Motor", "char=0.75 sway=0.8 soft=0.7 m_pedal=0.4 decay=0.7 space=0.3 size=0.7 trim=0.9" } } },
    { "Marimba", {
        /* Held notes roll; press to roll faster. */
        { "Marimba Roll", "char=0.6 trim=-5.3" },
        /* Hard mallets nearer the edge, a wooden attack, dry. */
        { "Marimba Hard Mallet", "soft=0.2 tone=0.6 m_noise=0.5 m_spot=0.3 decay=0.4 trim=3.4" } } },
    { "Xylophone", {
        /* Hard plastic mallets, bright and short, a small room. */
        { "Xylophone Hard", "soft=0.2 tone=0.65 m_spot=0.25 drive=0.1 size=0.3 trim=-0.3" },
        /* Held notes roll. */
        { "Xylophone Roll", "char=0.6 trim=-5.4" } } },
    { "Glockenspiel", {
        /* Brass mallets: the bell and the attack, damped quicker, close. */
        { "Glock Brass Mallets", "soft=0.3 char=0.7 tone=0.6 m_damp=0.3 decay=0.4 size=0.4 trim=-0.2" },
        /* Soft mallets far down the hall. */
        { "Glock Far Away", "soft=0.8 space=0.45 size=0.8 delay=0.35 trim=-0.4" } } },
    { "Tubular Bells", {
        /* A hand on the tube after each strike. */
        { "Chimes Muted", "char=0.6 trim=7.7" },
        /* Up in the cathedral's tower. */
        { "Chimes Cathedral", "space=0.5 size=0.9 delay=0.4 trim=-3.8" } } },
    { "Handbells", {
        /* The tierce out: a bright major ring, close, the hand on the bell. */
        { "Handbells Major", "char=0 soft=0.4 tone=0.58 m_split=0.05 m_damp=0.3 decay=0.45 size=0.45 trim=4.0" },
        /* The tierce in fully, a big church. */
        { "Handbells Minor", "char=1 space=0.45 size=0.8 trim=-3.2" } } },
    { "Handpan", {
        /* The note fields alone, a muted hand, close and warm. */
        { "Handpan Pure", "char=0.1 soft=0.75 tone=0.45 m_damp=0.35 m_body=0.75 trim=2.9" },
        /* The harmonics singing, a larger room. */
        { "Handpan Shimmer", "char=0.85 space=0.42 trim=-3.9" } } },
    { "Tongue Drum", {
        /* A pure tongue under a resting hand, short and dry. */
        { "Tongue Drum Pure", "char=0 soft=0.75 tone=0.45 m_damp=0.4 decay=0.4 trim=1.7" },
        /* The tongues ringing on with their harmonic. */
        { "Tongue Drum Ring", "char=0.85 decay=0.65 trim=-0.6" } } },
    { "Kalimba / Music Box", {
        /* Buzzers off, a little steel comb on a box, tinny and ringing. */
        { "Music Box", "char=0 soft=0.4 tone=0.62 m_spot=0.6 m_body=0.3 decay=0.7 space=0.2 size=0.4 trim=1.6" },
        /* The bottle-cap buzzers just in, a firm thumb, short. */
        { "Kalimba Buzz", "char=0.25 soft=0.4 m_damp=0.3 decay=0.4 trim=2.2" } } },

    /* Strings */
    { "Harp", {
        /* Close and dry, plucked firmly near the edge, the hand muting. */
        { "Harp Close", "char=0.2 soft=0.4 tone=0.56 p_edge=0.6 p_damp=0.3 decay=0.38 trim=11.6" },
        /* A small lever harp: lighter body, plucked nearer the end, shorter. */
        { "Lever Harp", "p_body=0.4 p_spot=0.2 p_edge=0.55 p_damp=0.2 decay=0.36 trim=10.8" } } },
    { "Nylon Guitar", {
        /* Over the fingerboard with the flesh of the thumb, the strings muted. */
        { "Nylon Warm", "p_spot=0.6 p_edge=0 soft=0.9 tone=0.38 p_damp=0.45 decay=0.4 space=0.15 trim=-1.6" },
        /* Near the bridge with the nail, the open strings ringing, dry. */
        { "Nylon Bright", "p_spot=0.08 p_edge=0.6 soft=0.35 tone=0.6 char=0.6 trim=-1.4" } } },
    { "Hammered Dulcimer", {
        /* Padded hammers, the courses in tune, the strings damped a little. */
        { "Dulcimer Soft", "soft=0.85 char=0.3 tone=0.42 m_split=0.2 m_damp=0.35 space=0.2 trim=0.8" },
        /* Bare hammers near the bridge, the courses apart, the strings blooming. */
        { "Dulcimer Bright", "soft=0.3 char=0.85 tone=0.6 m_split=0.5 m_spot=0.08 trim=4.3" } } },
    { "Pizzicato", {
        /* Violas, short and light. */
        { "Pizz Violas", "char=0.15 decay=0.55 trim=3.7" },
        /* Cellos, round and long. */
        { "Pizz Cellos", "char=0.95 trim=0.2" } } },
    { "Solo Cello", {
        /* The bow holds the note by itself, swelling slowly, far back and dark. */
        { "Cello Auto Bow", "b_press=1 b_swell=0.6 char=0.85 tone=0.42 b_bite=0.15 space=0.3 size=0.7 trim=5.8" },
        /* Near the bridge: glassy and thin. */
        { "Cello Ponticello", "char=0.05 b_edge=0.55 b_noise=0.25 trim=-1.8" } } },
    { "Solo Violin", {
        /* The bow holds the note by itself, swelling slowly, in a hall. */
        { "Violin Auto Bow", "b_press=1 b_swell=0.6 char=0.6 tone=0.45 b_bite=0.1 space=0.3 size=0.7 trim=-1.5" },
        /* Over the fingerboard: flautando. */
        { "Violin Sul Tasto", "char=0.85 b_edge=0.3 trim=8.4" } } },
    { "String Section", {
        /* The bow holds the note by itself, a slow swell, a big hall. */
        { "Strings Auto Bow", "b_press=1 b_swell=0.7 char=0.7 tone=0.42 space=0.35 size=0.85 trim=0.9" },
        /* A few players close up, biting near the bridge, a small room. */
        { "Strings Close", "char=0.2 b_bite=0.5 b_edge=0.6 b_swell=0.1 tone=0.56 size=0.35 trim=4.5" } } },

    /* Glass and bowls */
    { "Bowed Vibes", {
        /* The bow holds the bar by itself, slowly, the motor turning. */
        { "Bowed Vibes Auto", "d_press=1 d_swell=0.7 d_blur=0.4 sway=0.4 space=0.3 trim=3.9" },
        /* A mallet tap under a quick bow, the bar damped sooner, close. */
        { "Bowed Vibes Struck", "d_hit=0.8 d_swell=0.1 d_noise=0.05 decay=0.3 space=0.12 trim=2.3" } } },
    { "Glass Harmonica", {
        /* The glasses sing by themselves, slowly, wavering in a big room. */
        { "Glass Harmonica Auto", "d_press=1 d_swell=0.65 d_blur=0.3 sway=0.35 space=0.35 size=0.85 trim=1.4" },
        /* A drier finger, quick to speak: more grain. */
        { "Glass Harmonica Dry", "char=0.1 d_noise=0.45 d_swell=0.1 d_blur=0 tone=0.56 size=0.35 trim=5.2" } } },
    { "Singing Bowl", {
        /* Struck only, clear, left to ring long. */
        { "Singing Bowl Struck", "d_hit=1 d_bow=0 d_swell=0 d_blur=0 d_noise=0 decay=0.65 tone=0.56 space=0.2 trim=8.9" },
        /* Rubbed by itself, slowly, the rim grainy and wavering. */
        { "Singing Bowl Auto", "d_press=1 d_swell=0.65 d_blur=0.6 d_noise=0.4 sway=0.5 tone=0.42 size=0.85 space=0.3 trim=1.1" } } },

    /* Breath and voice */
    { "Flute", {
        /* The breath holds the note by itself, swelling slowly, in a hall. */
        { "Flute Auto Breath", "a_press=1 a_swell=0.45 tone=0.45 space=0.3 size=0.75 trim=-0.5" },
        /* Lots of air, the lip open, a gentle breath, no vibrato. */
        { "Flute Breathy", "a_air=0.42 char=0.1 a_edge=0.2 a_swell=0.05 tone=0.55 soft=0.8 sway=0 trim=4.5" } } },
    { "Pan Flute", {
        /* The breath holds the note by itself, swelling slowly, far off. */
        { "Pan Flute Auto", "a_press=1 a_swell=0.6 tone=0.45 space=0.3 size=0.8 trim=-0.3" },
        /* Lots of air and puff, a gentle breath, a little vibrato. */
        { "Pan Flute Breathy", "a_air=0.45 char=0.9 a_edge=0.2 soft=0.8 sway=0.4 trim=1.4" } } },
    { "Ocarina", {
        /* The breath holds the note by itself, swelling slowly, a dark room. */
        { "Ocarina Auto", "a_press=1 a_swell=0.35 space=0.3 size=0.75 dark=0.6 trim=-0.6" },
        /* Breathier and brighter, less pure, a little vibrato. */
        { "Ocarina Breathy", "char=0.1 a_air=0.42 a_edge=0.6 tone=0.56 soft=0.8 sway=0.4 trim=3.0" } } },
    { "Recorder", {
        /* The breath holds the note by itself, swelling slowly, in a hall. */
        { "Recorder Auto", "a_press=1 a_swell=0.4 tone=0.45 space=0.3 size=0.75 trim=-1.3" },
        /* A brighter, chiffier voice flute, quick and close. */
        { "Recorder Bright", "char=0.9 a_edge=0.85 a_air=0.4 a_swell=0 tone=0.6 trim=-2.6" } } },
    { "Clarinet", {
        /* The breath holds the note by itself, a soft reed swelling slowly, far. */
        { "Clarinet Auto", "a_press=1 a_swell=0.6 char=0.1 tone=0.42 space=0.3 size=0.7 trim=-0.7" },
        /* A harder reed, the clarion's edge. */
        { "Clarinet Bright", "char=0.7 a_edge=0.75 tone=0.58 trim=3.1" } } },
    { "Choir", {
        /* The choir on ah. */
        { "Choir Ah", "char=0.9 trim=5.5" },
        /* One voice, close. */
        { "Choir Solo Voice", "v_crowd=1 v_split=0.1 space=0.35 trim=-0.7" } } },
};

#define NP ((int)(sizeof(PRESETS) / sizeof(PRESETS[0])))

/* The preset list's groups, alphabetical by instrument, so the list is
 * easy to find your way along; TYPE keeps its families. ORDER[g] is the
 * TYPE option whose presets are group g, worked out once. */
static int ORDER[QUILT_NTYPES], RANK[QUILT_NTYPES], ordered;
static int by_name(const void *a, const void *b) {
    return strcmp(QUILT_TYPE_NAMES[*(const int *)a], QUILT_TYPE_NAMES[*(const int *)b]);
}
static void order(void) {
    if (ordered) return;
    for (int t = 0; t < QUILT_NTYPES; t++) ORDER[t] = t;
    qsort(ORDER, QUILT_NTYPES, sizeof(int), by_name);
    for (int g = 0; g < QUILT_NTYPES; g++) RANK[ORDER[g]] = g;
    ordered = 1;
}

int quilt_preset_type(int preset) {
    if (preset < 0 || preset >= QUILT_NPRESETS) return -1;
    order();
    return ORDER[preset / QUILT_PER_TYPE];
}

int quilt_type_first_preset(int t) {
    if (t < 0 || t >= QUILT_NTYPES) return -1;
    order();
    return RANK[t] * QUILT_PER_TYPE;
}

static int row(int preset) {
    if (preset < 0 || preset >= QUILT_NPRESETS) return -1;
    const char *inst = QUILT_TYPE_NAMES[quilt_preset_type(preset)];
    for (int i = 0; i < NP; i++)
        if (!strcmp(PRESETS[i].inst, inst)) return i;
    return -1;
}

const char *quilt_preset_name(int preset) {
    int r = row(preset), v = preset % QUILT_PER_TYPE;
    if (r < 0 || v == 0) return preset >= 0 && preset < QUILT_NPRESETS ? QUILT_TYPE_NAMES[quilt_preset_type(preset)] : "";
    return PRESETS[r].v[v - 1].name;
}

const char *quilt_preset_changes(int preset) {
    int r = row(preset), v = preset % QUILT_PER_TYPE;
    return r < 0 || v == 0 ? "" : PRESETS[r].v[v - 1].changes;
}
