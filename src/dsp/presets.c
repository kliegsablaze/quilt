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
        /* The strip lifted out: the same close upright, hammers bare. */
        { "Upright Bare Hammer", "char=0.05 soft=0.45 m_noise=0.4 tone=0.5 trim=0.5" },
        /* A thick strip, the pedal half down, the room a little larger. */
        { "Felt Lullaby", "char=0.7 soft=0.7 m_pedal=0.35 decay=0.6 space=0.26 size=0.5 dark=0.6 trim=2.2" } } },
    { "Una Corda Grand", {
        /* The soft pedal up: all three strings, the lid open, nearer. */
        { "Grand Open Lid", "char=0.05 soft=0.42 m_noise=0.3 space=0.22 size=0.55 trim=2.5" },
        /* Further back in the hall, the sustain pedal half down. */
        { "Grand Far Hall", "char=0.6 m_pedal=0.45 space=0.42 size=0.8 delay=0.4 dark=0.55 trim=-2.6" } } },
    { "Electric Grand", {
        /* Played out, pickups near the bridge, no pan. */
        { "E.Grand Bright", "char=0.6 soft=0.4 sway=0 tone=0.58 trim=-0.7" },
        /* Softly, with a slow wide auto-pan and more room. */
        { "E.Grand Drift", "char=0.2 soft=0.65 sway=0.5 speed=0.18 space=0.3 trim=0.5" } } },
    { "Clavichord", {
        /* The lute stop: cloth on the strings, short and plucked. */
        { "Clavichord Lute", "p_damp=0.85 p_edge=0.4 char=0.3 decay=0.3 trim=1.9" },
        /* Bebung deep, so pressing a held key sings and wavers. */
        { "Clavichord Bebung", "char=0.9 p_damp=0.35 decay=0.5 space=0.2 trim=-1.4" } } },
    { "Tine Piano", {
        /* Tines voiced away from the pickups: round and bell-like. */
        { "Tine Piano Bell", "char=0.1 soft=0.68 m_split=0.15 sway=0.2 trim=0.3" },
        /* Voiced close, played harder, the amplifier pushed. */
        { "Tine Piano Bark", "char=0.8 soft=0.45 drive=0.3 sway=0.15 trim=-1.7" } } },
    { "Reed Piano", {
        /* A wide pickup gap and no tremolo: sweet and clean. */
        { "Reed Piano Sweet", "char=0.2 soft=0.7 sway=0 drive=0.05 trim=0.4" },
        /* The tremolo up and quicker, the reeds biting. */
        { "Reed Piano Tremolo", "char=0.6 sway=0.6 speed=0.55 drive=0.2 trim=1.0" } } },
    { "Celesta", {
        /* Harder hammers, more of the bell. */
        { "Celesta Bright", "char=0.7 soft=0.45 tone=0.55 trim=1.7" },
        /* Soft, the pedal half down, a large room. */
        { "Celesta Dream", "char=0.3 soft=0.7 m_pedal=0.5 space=0.45 size=0.75 delay=0.3 trim=-1.1" } } },
    { "Toy Piano", {
        /* Rods well out of tune with each other, very clangy. */
        { "Toy Piano Broken", "m_split=0.45 char=0.75 trim=0.4" },
        /* Played gently in a bedroom. */
        { "Toy Piano Lullaby", "soft=0.65 char=0.3 space=0.32 size=0.45 trim=-0.5" } } },
    { "Tonewheel Organ", {
        /* 88 8000 000: the gospel ballad, Leslie slow. */
        { "Organ 888 Ballad", "o_16=8 o_513=8 o_8=8 o_4=0 char=0.5 sway=0.1 trim=-2.7" },
        /* 00 8000 000 with third percussion, scanner off: jazz. */
        { "Organ Jazz Perc", "o_8=8 o_4=0 o_ping=0.7 o_tail=0.3 o_click=0.45 char=0 sway=0 trim=1.2" } } },
    { "Flute Organ", {
        /* The stopped 8' alone, quiet and hollow. */
        { "Flute Organ Gedackt", "k_8=0.9 k_4=0 k_2=0 char=0.3 k_edge=0.15 trim=1.8" } ,
        /* 8', 4' and 2' drawn: the small plenum. */
        { "Flute Organ Full", "k_8=0.7 k_4=0.6 k_2=0.5 char=0.55 k_edge=0.4 trim=2.0" } } },
    { "Harmonium", {
        /* The céleste rank fully in: a slow beating shimmer. */
        { "Harmonium Celeste", "char=0.85 trim=0.6" },
        /* Further off, breathier, swelling slowly. */
        { "Harmonium Distant", "char=0.25 a_air=0.32 a_swell=0.5 space=0.38 size=0.7 trim=-1.9" } } },
    { "Glass E.Piano", {
        /* More of the bell pair, played a little harder. */
        { "Glass EP Bell", "char=0.7 f_tine=0.55 soft=0.5 trim=1.5" },
        /* Soft, little tine, the chorus wider. */
        { "Glass EP Soft", "char=0.15 soft=0.75 f_tine=0.3 sway=0.35 trim=2.6" } } },
    { "String Ensemble", {
        /* The 8' alone, darker: cellos and violas. */
        { "Ensemble Low", "k_8=0.9 k_4=0.2 k_2=0 k_edge=0.2 k_swell=0.55 trim=2.8" },
        /* The 4' and 2' over the 8': the violins on top. */
        { "Ensemble High", "k_8=0.45 k_4=0.8 k_2=0.35 k_edge=0.4 trim=2.5" } } },

    /* Mallets */
    { "Vibraphone", {
        /* The fan stopped: a pure ring. */
        { "Vibes Motor Off", "sway=0 m_damp=0.2 trim=-0.1" },
        /* The fan fast and deep, the old ballroom shimmer. */
        { "Vibes Fast Motor", "char=0.7 sway=0.6 trim=2.6" } } },
    { "Marimba", {
        /* Held notes roll; press to roll faster. */
        { "Marimba Roll", "char=0.6 trim=-5.9" },
        /* Harder mallets, a wooden attack. */
        { "Marimba Hard Mallet", "soft=0.3 m_noise=0.4 trim=0.6" } } },
    { "Xylophone", {
        /* Hard plastic mallets, bright and short. */
        { "Xylophone Hard", "soft=0.35 tone=0.55 trim=0.3" },
        /* Held notes roll. */
        { "Xylophone Roll", "char=0.6 trim=-5.9" } } },
    { "Glockenspiel", {
        /* Brass mallets: the bell and the attack. */
        { "Glock Brass Mallets", "soft=0.45 char=0.55 trim=-0.3" },
        /* Soft mallets far down the hall. */
        { "Glock Far Away", "soft=0.8 space=0.45 size=0.8 delay=0.35 trim=-0.4" } } },
    { "Tubular Bells", {
        /* A hand on the tube after each strike. */
        { "Chimes Muted", "char=0.6 trim=7.4" },
        /* Up in the cathedral's tower. */
        { "Chimes Cathedral", "space=0.5 size=0.9 delay=0.4 trim=-3.8" } } },
    { "Handbells", {
        /* The tierce out: a bright major ring. */
        { "Handbells Major", "char=0 trim=2.6" },
        /* The tierce in fully, a big church. */
        { "Handbells Minor", "char=1 space=0.45 size=0.8 trim=-3.2" } } },
    { "Handpan", {
        /* The note fields alone, little overtone ring. */
        { "Handpan Pure", "char=0.2 trim=2.7" },
        /* The harmonics singing, a larger room. */
        { "Handpan Shimmer", "char=0.85 space=0.42 trim=-3.9" } } },
    { "Tongue Drum", {
        /* A pure tongue, the box quiet. */
        { "Tongue Drum Pure", "char=0.15 trim=1.0" },
        /* The tongues ringing on with their harmonic. */
        { "Tongue Drum Ring", "char=0.85 decay=0.65 trim=-0.8" } } },
    { "Kalimba / Music Box", {
        /* Buzzers off, a steel comb: the music box. */
        { "Music Box", "char=0 soft=0.7 trim=0.9" },
        /* The bottle-cap buzzers rattling. */
        { "Kalimba Buzz", "char=1 trim=0.8" } } },

    /* Strings */
    { "Harp", {
        /* Close, with little of the hall. */
        { "Harp Close", "char=0.2 space=0.15 size=0.4 trim=5.9" },
        /* A small lever harp: lighter body, plucked nearer the end. */
        { "Lever Harp", "p_body=0.4 p_spot=0.2 p_edge=0.55 p_damp=0.2 decay=0.55 trim=6.8" } } },
    { "Nylon Guitar", {
        /* Over the fingerboard with the flesh of the thumb. */
        { "Nylon Warm", "p_spot=0.4 p_edge=0.1 soft=0.7 trim=-0.2" },
        /* Near the bridge with the nail. */
        { "Nylon Bright", "p_spot=0.12 p_edge=0.45 soft=0.45 trim=0.8" } } },
    { "Hammered Dulcimer", {
        /* Padded hammers. */
        { "Dulcimer Soft", "soft=0.8 char=0.4 trim=1.1" },
        /* Bare hammers, the strings blooming. */
        { "Dulcimer Bright", "soft=0.4 char=0.75 trim=0.9" } } },
    { "Pizzicato", {
        /* Violas, short and light. */
        { "Pizz Violas", "char=0.15 decay=0.55 trim=3.3" },
        /* Cellos, round and long. */
        { "Pizz Cellos", "char=0.95 trim=-0.3" } } },
    { "Solo Cello", {
        /* The bow holds the note by itself; no pressing needed. */
        { "Cello Auto Bow", "b_press=1 trim=1.9" },
        /* Near the bridge: glassy and thin. */
        { "Cello Ponticello", "char=0.05 b_edge=0.55 b_noise=0.25 trim=-2.1" } } },
    { "Solo Violin", {
        /* The bow holds the note by itself. */
        { "Violin Auto Bow", "b_press=1 trim=1.8" },
        /* Over the fingerboard: flautando. */
        { "Violin Sul Tasto", "char=0.85 b_edge=0.3 trim=8.3" } } },
    { "String Section", {
        /* The bow holds the note by itself. */
        { "Strings Auto Bow", "b_press=1 trim=2.8" },
        /* A few players close up, a smaller room. */
        { "Strings Close", "char=0.25 space=0.25 size=0.45 trim=2.3" } } },

    /* Glass and bowls */
    { "Bowed Vibes", {
        /* The bow holds the bar by itself. */
        { "Bowed Vibes Auto", "d_press=1 trim=2.4" },
        /* A mallet tap under the bow. */
        { "Bowed Vibes Struck", "d_hit=0.8 trim=2.4" } } },
    { "Glass Harmonica", {
        /* The glasses sing by themselves. */
        { "Glass Harmonica Auto", "d_press=1 trim=2.5" },
        /* A drier finger: more grain. */
        { "Glass Harmonica Dry", "char=0.15 d_noise=0.2 trim=4.1" } } },
    { "Singing Bowl", {
        /* Struck only, left to ring. */
        { "Singing Bowl Struck", "d_hit=1 d_bow=0.15 trim=6.4" },
        /* Rubbed by itself, no pressing needed. */
        { "Singing Bowl Auto", "d_press=1 trim=3.3" } } },

    /* Breath and voice */
    { "Flute", {
        /* The breath holds the note by itself. */
        { "Flute Auto Breath", "a_press=1 trim=2.0" },
        /* Lots of air, the lip open. */
        { "Flute Breathy", "a_air=0.4 char=0.15 trim=0.0" } } },
    { "Pan Flute", {
        /* The breath holds the note by itself. */
        { "Pan Flute Auto", "a_press=1 trim=2.4" },
        /* Lots of air and puff. */
        { "Pan Flute Breathy", "a_air=0.45 char=0.85 a_edge=0.3 trim=2.3" } } },
    { "Ocarina", {
        /* The breath holds the note by itself. */
        { "Ocarina Auto", "a_press=1 trim=2.5" },
        /* Breathier, less pure. */
        { "Ocarina Breathy", "char=0.2 a_air=0.34 trim=2.1" } } },
    { "Recorder", {
        /* The breath holds the note by itself. */
        { "Recorder Auto", "a_press=1 trim=2.0" },
        /* A brighter, chiffier voice flute. */
        { "Recorder Bright", "char=0.65 a_edge=0.6 trim=-1.9" } } },
    { "Clarinet", {
        /* The breath holds the note by itself. */
        { "Clarinet Auto", "a_press=1 trim=1.8" },
        /* A harder reed, the clarion's edge. */
        { "Clarinet Bright", "char=0.65 a_edge=0.55 trim=1.6" } } },
    { "Choir", {
        /* The choir on ah. */
        { "Choir Ah", "char=0.9 trim=5.2" },
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
